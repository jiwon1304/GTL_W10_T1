#include "FFbxLoader.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include "FbxObject.h"
#include "Serializer.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimData/AnimDataModel.h"
#include "UObject/ObjectFactory.h"
#include "Components/Material/Material.h"
#include "Engine/Asset/SkeletalMeshAsset.h"
#include "Components/Mesh/SkeletalMesh.h"
#include "Container/StringConv.h"
#include "Engine/AssetManager.h"

#define DEBUG_DUMP_ANIMATION
struct BoneWeights
{
    int jointIndex;
    float weight;
};

void FFbxLoader::Init()
{
    if (!Manager)
    {
        Manager = FbxManager::Create();
    }
}

// FBX 파일을 로드합니다.
// 비동기적으로 실행되며, 실행이 끝나면 로그와 함께 UAssetManager에 해당 등록됩니다.
// 현재는 UAssetManager에서 Contents 폴더의 모든 파일에 대해서 프로그램 시작 시 호출됩니다.
void FFbxLoader::LoadFBX(const FString& filename)
{
    UE_LOG(ELogLevel::Display, "Loading FBX : %s", *filename);
    {
        std::lock_guard<std::mutex> lock(MapMutex);
        if (MeshMap.Contains(filename)) return;

        // 바로 Loading 상태 등록
        MeshMap.Add(filename, { LoadState::Loading, nullptr });
    }

    std::thread loader([filename]() {
        USkeletalMesh* mesh = ParseSkeletalMesh(filename);
        std::lock_guard<std::mutex> lock(MapMutex);
        if (mesh) {
            MeshMap[filename] = { LoadState::Completed, mesh };
        }
        else
        {
            MeshMap[filename] = { LoadState::Failed, nullptr };
        }
        OnLoadFBXCompleted.Execute(filename);
        });
    loader.detach();
}

// 이전에 LoadFBX로 호출된 파일이라면 로드된 에셋을 반환합니다.
// 만약 그런적이 없다면 메인 쓰레드에서 로드합니다.
USkeletalMesh* FFbxLoader::GetSkeletalMesh(const FString& filename)
{
    while (true)
    {
        {
            std::lock_guard<std::mutex> lock(MapMutex);

            // 로드를 시도했으면 기다림
            if (MeshMap.Contains(filename))
            {
                const MeshEntry& entry = MeshMap[filename];
                switch (entry.State)
                {
                case LoadState::Completed:
                    return entry.Mesh;
                case LoadState::Failed:
                    return nullptr;
                case LoadState::Loading:
                    break; //switch break : 기다림
                }
            }
            else
            {
                break; // while break : 메인 쓰레드에서 로드
            }
        }

        // Sleep 없이 무한 루프 → CPU 낭비 방지
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 로드를 시작한 적이 없으면 메인쓰레드에서 로드
    // 이 경우 AssetManager에서 로드한 적이 없는거이므로
    // AssetManager에 추가

    TMap<FName, FAssetInfo> AssetRegistry = UAssetManager::Get().GetAssetRegistry();
    bool bRegistered = false;
    for (const auto& Asset : AssetRegistry)
    {
        if (Asset.Value.GetFullPath() == filename)
        {
            bRegistered = true;
            break;
        }

    }
    // 만약 등록되지 않았으면 등록하고 로드
    if (!bRegistered)
    {
        UAssetManager::Get().RegisterAsset(StringToWString(*filename));
    }
    USkeletalMesh* mesh = nullptr;
    {
        // 메인쓰레드에서 실행
        mesh = ParseSkeletalMesh(filename);
        std::lock_guard<std::mutex> lock(MapMutex);
        if (mesh) {
            MeshMap[filename] = { LoadState::Completed, mesh };
        }
        else
        {
            MeshMap[filename] = { LoadState::Failed, nullptr };
        }
    }
    
    return mesh;
}

// .fbx 파일을 파싱합니다.
FFbxSkeletalMesh* FFbxLoader::ParseFBX(const FString& FBXFilePath, USkeletalMesh* Mesh)
{
    UE_LOG(ELogLevel::Display, "Start FBX Parsing : %s", *FBXFilePath);
    // .fbx 파일을 로드/언로드 시에만 mutex를 사용
    FbxScene* scene = nullptr;
    FbxGeometryConverter* converter;

    scene = FbxScene::Create(FFbxLoader::Manager, "");
    FbxImporter* importer = FbxImporter::Create(Manager, "");

    if (!importer->Initialize(GetData(FBXFilePath), -1, GetFbxIOSettings()))
    {
        importer->Destroy();
        return nullptr;
    }

    if (importer->IsFBX())
    {
        FbxIOSettings* iosettings = GetFbxIOSettings();
        iosettings->SetBoolProp(IMP_FBX_MATERIAL, true);
        iosettings->SetBoolProp(IMP_FBX_TEXTURE, true);
        iosettings->SetBoolProp(IMP_FBX_LINK, true);
        iosettings->SetBoolProp(IMP_FBX_SHAPE, true);
        iosettings->SetBoolProp(IMP_FBX_GOBO, true);
        iosettings->SetBoolProp(IMP_FBX_ANIMATION, true);
        iosettings->SetBoolProp(IMP_FBX_GLOBAL_SETTINGS, true);
    }

    bool bIsImported = importer->Import(scene);
    importer->Destroy();
    if (!bIsImported)
    {
        return nullptr;
    }

    // convert scene
    FbxAxisSystem sceneAxisSystem = scene->GetGlobalSettings().GetAxisSystem();
    FbxAxisSystem targetAxisSystem(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityEven, FbxAxisSystem::eLeftHanded);
    if (sceneAxisSystem != targetAxisSystem)
    {
        targetAxisSystem.ConvertScene(scene);
    }
    
    FbxSystemUnit SceneSystemUnit = scene->GetGlobalSettings().GetSystemUnit();
    if( SceneSystemUnit.GetScaleFactor() != 1.0 )
    {
        FbxSystemUnit::cm.ConvertScene(scene);
    }
    
    converter = new FbxGeometryConverter(Manager);
    converter->Triangulate(scene, true);
    delete converter;

    FFbxSkeletalMesh* result;

    result = LoadFBXObject(scene, Mesh);
    scene->Destroy();
    result->name = FBXFilePath;

    if (Mesh)
    {
        TArray<UAnimSequence*> AnimSequences;
        LoadAnimationInfo(scene, Mesh, AnimSequences);
        for (UAnimSequence* Sequence : AnimSequences)
        {
            LoadAnimationData(scene, scene->GetRootNode(), Mesh, Sequence);
        }
#ifdef DEBUG_DUMP_ANIMATION
        DumpAnimationDebug(FBXFilePath, Mesh, AnimSequences);
#endif
    }
    return result;
}

// Skeletal Mesh를 파싱합니다.
// 등록되지 않은 .bin 또는 .fbx 파일을 파싱합니다.
USkeletalMesh* FFbxLoader::ParseSkeletalMesh(const FString& filename)
{
    FWString BinaryPath = (filename + ".bin").ToWideString();

    // Last Modified Time
    auto FileTime = std::filesystem::last_write_time(filename.ToWideString());
    int64_t lastModifiedTime = std::chrono::system_clock::to_time_t(
    std::chrono::time_point_cast<std::chrono::system_clock::duration>(
    FileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()));
    
    // fbx 파일에서 바로 추출한 데이터. 엔진에서 사용할 수 있게 USkeletalMesh로 변환해야함.
    FFbxSkeletalMesh* fbxObject = new FFbxSkeletalMesh();
    bool bCreateNewMesh = true;
    
    // bin 파일이 존재하면 로드
    if (std::ifstream(BinaryPath).good())
    {
        // bin
        if (FFbxManager::LoadFBXFromBinary(BinaryPath, lastModifiedTime, *fbxObject))
        {
            bCreateNewMesh = false;
        }
    }

    // bin 파일 없음. fbx 파싱 필요
    if (bCreateNewMesh)
    {
        std::lock_guard<std::mutex> lock(SDKMutex);
        fbxObject = ParseFBX(filename, nullptr);
        if (fbxObject)
        {
            FFbxManager::SaveFBXToBinary(BinaryPath, lastModifiedTime, *fbxObject);
        }
    }
    
    if (!fbxObject) // 파싱 실패
    {
        delete fbxObject;
        return nullptr;
    }

    // .bin 또는 .fbx 파일에서 파싱한 FFbxSkeletalMesh를 USkeletalMesh로 변환
    USkeletalMesh* newSkeletalMesh = FObjectFactory::ConstructObject<USkeletalMesh>(nullptr);

    FSkeletalMeshRenderData renderData;
    renderData.ObjectName = fbxObject->name;
    renderData.RenderSections.SetNum(fbxObject->mesh.Num());
    renderData.MaterialSubsets.SetNum(fbxObject->materialSubsets.Num());

    for (int i = 0; i < fbxObject->mesh.Num(); ++i)
    {
        // TArray로 직접 접근해도 돼나?
        // 두 구조체의 메모리 레이아웃이 같아야함.
        renderData.RenderSections[i].Vertices.SetNum(fbxObject->mesh[i].vertices.Num());
        for (int j = 0; j < fbxObject->mesh[i].vertices.Num(); ++j)
        {
            renderData.RenderSections[i].Vertices[j].Position = fbxObject->mesh[i].vertices[j].position;
            renderData.RenderSections[i].Vertices[j].Color = fbxObject->mesh[i].vertices[j].color;
            renderData.RenderSections[i].Vertices[j].Normal = fbxObject->mesh[i].vertices[j].normal;
            renderData.RenderSections[i].Vertices[j].Tangent = fbxObject->mesh[i].vertices[j].tangent;
            renderData.RenderSections[i].Vertices[j].UV = fbxObject->mesh[i].vertices[j].uv;
            renderData.RenderSections[i].Vertices[j].MaterialIndex = fbxObject->mesh[i].vertices[j].materialIndex;
            memcpy(
                renderData.RenderSections[i].Vertices[j].BoneIndices,
                fbxObject->mesh[i].vertices[j].boneIndices,
                sizeof(int8) * 8
            );
            memcpy(
                renderData.RenderSections[i].Vertices[j].BoneWeights,
                fbxObject->mesh[i].vertices[j].boneWeights,
                sizeof(float) * 8
            );
        }
        memcpy(
            renderData.RenderSections[i].Vertices.GetData(),
            fbxObject->mesh[i].vertices.GetData(),
            sizeof(FFbxVertex) * fbxObject->mesh[i].vertices.Num()
        );

        renderData.RenderSections[i].Indices = fbxObject->mesh[i].indices;
        renderData.RenderSections[i].SubsetIndex = fbxObject->mesh[i].subsetIndex;
        renderData.RenderSections[i].Name = fbxObject->mesh[i].name;
    }
    for (int i = 0; i < fbxObject->materialSubsets.Num(); ++i)
    {
        renderData.MaterialSubsets[i] = fbxObject->materialSubsets[i];
    }
    //RenderDatas.Add(renderData);

    FReferenceSkeleton refSkeleton;
    refSkeleton.RawRefBoneInfo.SetNum(fbxObject->skeleton.joints.Num());
    refSkeleton.RawRefBonePose.SetNum(fbxObject->skeleton.joints.Num());
    //refSkeleton.RawNameToIndexMap.Reserve(fbxObject->skeleton.joints.Num());
    for (int i = 0; i < fbxObject->skeleton.joints.Num(); ++i)
    {
        refSkeleton.RawRefBoneInfo[i].Name = fbxObject->skeleton.joints[i].name;
        refSkeleton.RawRefBoneInfo[i].ParentIndex = fbxObject->skeleton.joints[i].parentIndex;
        refSkeleton.RawRefBonePose[i].Translation = fbxObject->skeleton.joints[i].position;
        refSkeleton.RawRefBonePose[i].Rotation = fbxObject->skeleton.joints[i].rotation;
        refSkeleton.RawRefBonePose[i].Scale3D = fbxObject->skeleton.joints[i].scale;
        refSkeleton.RawNameToIndexMap.Add(fbxObject->skeleton.joints[i].name, i);
    }

    TArray<UMaterial*> Materials = fbxObject->material;

    TArray<FMatrix> InverseBindPoseMatrices;
    InverseBindPoseMatrices.SetNum(fbxObject->skeleton.joints.Num());
    for (int i = 0; i < fbxObject->skeleton.joints.Num(); ++i)
    {
        InverseBindPoseMatrices[i] = fbxObject->skeleton.joints[i].inverseBindPose;
    }
    newSkeletalMesh->SetData(renderData, refSkeleton, InverseBindPoseMatrices, Materials);
    if (InverseBindPoseMatrices.Num() > 128)
    {
        // GPU Skinning: 최대 bone 개수 128개를 넘어가면 CPU로 전환
        newSkeletalMesh->bCPUSkinned = true;
    }

    {
        std::lock_guard<std::mutex> lock(SDKMutex);
        ParseFBXAnimationOnly(filename, newSkeletalMesh); // 이 호출에서 애니메이션 정보만 파싱됨
    }

    delete fbxObject;
    return newSkeletalMesh;
}

FFbxSkeletalMesh* FFbxLoader::LoadFBXObject(FbxScene* InFbxScene, USkeletalMesh* Mesh)
{
    FFbxSkeletalMesh* result = new FFbxSkeletalMesh();

    TArray<TMap<int, TArray<BoneWeights>>> weightMaps;
    TMap<FString, int> boneNameToIndex;

    TArray<FbxNode*> skeletons;
    TArray<FbxNode*> meshes;
    
    std::function<void(FbxNode*)> Traverse = [&](FbxNode* Node)
    {   if (!Node) return;
        FbxNodeAttribute* attr = Node->GetNodeAttribute();
        if (attr)
        {
            switch (attr->GetAttributeType())
            {
            case FbxNodeAttribute::eSkeleton:
                skeletons.Add(Node);
                break;
            case FbxNodeAttribute::eMesh:
                meshes.Add(Node);
                break;
            default:
                break;
            }
        }
        for (int i = 0; i < Node->GetChildCount(); ++i)
        {
            Traverse(Node->GetChild(i));
        }
    };
    
    Traverse(InFbxScene->GetRootNode());

    // parse bones
    for (auto& node : skeletons)
    {
        LoadFbxSkeleton(result, node, boneNameToIndex, -1);
    }

    // parse skins
    for (int i = 0; i < meshes.Num(); ++i)
    {
        FbxNode*& node = meshes[i];
        TMap<int, TArray<BoneWeights>> weightMap;
        LoadSkinWeights(node, boneNameToIndex, weightMap);
        weightMaps.Add(weightMap);
    }

    // parse meshes & material
    for (int i = 0; i < meshes.Num(); ++i)
    {
        FbxNode*& node = meshes[i];
        LoadFBXMesh(result, node, boneNameToIndex, weightMaps[i]);
        LoadFBXMaterials(result, node);
    }

//    // 애니메이션 정보 로드
//    TArray<UAnimSequence*> AnimSequences;
//    LoadAnimationInfo(InFbxScene, Mesh, AnimSequences);
//
//    // 키프레임 데이터 로드
//    for (UAnimSequence* Sequence : AnimSequences)
//    {
//        LoadAnimationData(InFbxScene, InFbxScene->GetRootNode(), Mesh, Sequence);
//    }
//
//#ifdef DEBUG_DUMP_ANIMATION
//    DumpAnimationDebug(result->name, Mesh, AnimSequences);
//#endif

    return result;
}


FbxIOSettings* FFbxLoader::GetFbxIOSettings()
{
    if (Manager->GetIOSettings() == nullptr)
    {
        Manager->SetIOSettings(FbxIOSettings::Create(Manager, "IOSRoot"));
    }
    return Manager->GetIOSettings();
}

FbxCluster* FFbxLoader::FindClusterForBone(FbxNode* boneNode)
{
    if (!boneNode || !boneNode->GetScene()) return nullptr;
    FbxScene* scene = boneNode->GetScene();

    for (int i = 0; i < scene->GetRootNode()->GetChildCount(); ++i)
    {
        FbxNode* meshNode = scene->GetRootNode()->GetChild(i);
        FbxMesh* mesh = meshNode ? meshNode->GetMesh() : nullptr;
        if (!mesh) continue;

        int skinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
        for (int s = 0; s < skinCount; ++s)
        {
            FbxSkin* skin = static_cast<FbxSkin*>(mesh->GetDeformer(s, FbxDeformer::eSkin));
            for (int c = 0; c < skin->GetClusterCount(); ++c)
            {
                FbxCluster* cluster = skin->GetCluster(c);
                if (cluster->GetLink() == boneNode)
                    return cluster;
            }
        }
    }
    return nullptr;
}





void FFbxLoader::LoadFbxSkeleton(
    FFbxSkeletalMesh* fbxObject,
    FbxNode* node,
    TMap<FString, int>& boneNameToIndex,
    int parentIndex = -1
)
{
    if (!node || boneNameToIndex.Contains(node->GetName()))
        return;

    FbxNodeAttribute* attr = node->GetNodeAttribute();
    if (!attr || attr->GetAttributeType() != FbxNodeAttribute::eSkeleton)
    {
        for (int i = 0; i < node->GetChildCount(); ++i)
        {
            LoadFbxSkeleton(fbxObject, node->GetChild(i), boneNameToIndex, parentIndex);
        }
        return;
    }

    FFbxJoint joint;
    joint.name = node->GetName();
    joint.parentIndex = parentIndex;

    FbxCluster* cluster = FindClusterForBone(node);
    FbxPose* pose = nullptr;
    for (int i = 0; i < node->GetScene()->GetPoseCount(); ++i)
    {
        pose = node->GetScene()->GetPose(i);
        if (pose->IsBindPose())
            break;
    }
    
    // https://blog.naver.com/jidon333/220264383892
    // Mesh -> Deformer -> Cluster -> Link == "joint"
    // bone은 joint사이의 공간을 말하는거지만, 사실상 joint와 동일한 의미로 사용되고 있음.
    if (cluster)
    {
        // Inverse Pose Matrix를 구함
        FbxAMatrix LinkMatrix, Matrix;
        cluster->GetTransformLinkMatrix(LinkMatrix);  // !!! 실제 joint Matrix : joint->model space 변환 행렬
        cluster->GetTransformMatrix(Matrix);      // Fbx 모델의 전역 오프셋 : 모든 joint가 같은 값을 가짐
        FbxAMatrix InverseMatrix = LinkMatrix.Inverse() * Matrix;

        FbxAMatrix bindLocal = node->EvaluateLocalTransform();

        // FBX 행렬을 Unreal 형식으로 복사
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                joint.localBindPose.M[i][j] = static_cast<float>(bindLocal[i][j]);

        // FBX 행렬을 Unreal 형식으로 복사
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                joint.inverseBindPose.M[i][j] = static_cast<float>(InverseMatrix[i][j]);
    }
    // FbxPose를 통해 구하는 방법?
    else if (pose)
    {
        int index = pose->Find(node);
        if (index >= 0)
        {
            FbxMatrix bindLocal = pose->GetMatrix(index);
            FbxMatrix inverseBindLocal = bindLocal.Inverse();
            
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    joint.localBindPose.M[i][j] = static_cast<float>(bindLocal[i][j]);

            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    joint.inverseBindPose.M[i][j] = static_cast<float>(inverseBindLocal[i][j]);
        }
    }
    else
    {
        // 클러스터가 없는 경우에는 fallback으로 EvaluateLocalTransform 사용 (확인안됨)
        FbxAMatrix localMatrix = node->EvaluateLocalTransform();
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                joint.localBindPose.M[i][j] = static_cast<float>(localMatrix[i][j]);

        FbxAMatrix globalMatrix = node->EvaluateGlobalTransform();
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                joint.inverseBindPose.M[i][j] = static_cast<float>(globalMatrix[i][j]);
        joint.inverseBindPose = FMatrix::Inverse(joint.inverseBindPose);
    }

    FbxAMatrix LocalTransform = node->EvaluateLocalTransform();
    FMatrix Mat;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            Mat.M[i][j] = static_cast<float>(LocalTransform[i][j]);
    
    FTransform Transform(Mat);

    joint.position = Transform.Translation;
    joint.rotation = Transform.Rotation;
    joint.scale = Transform.Scale3D;

    int thisIndex = fbxObject->skeleton.joints.Num();
    fbxObject->skeleton.joints.Add(joint);
    boneNameToIndex.Add(joint.name, thisIndex);

    // 재귀적으로 하위 노드 순회
    for (int i = 0; i < node->GetChildCount(); ++i)
    {
        LoadFbxSkeleton(fbxObject, node->GetChild(i), boneNameToIndex, thisIndex);
    }
}

void FFbxLoader::LoadSkinWeights(
    FbxNode* node,
    const TMap<FString, int>& boneNameToIndex,
    TMap<int, TArray<BoneWeights>>& OutBoneWeights
)
{
    FbxMesh* mesh = node->GetMesh();
    if (!mesh) return;
    
    int skinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
    for (int s = 0; s < skinCount; ++s)
    {
        FbxSkin* skin = static_cast<FbxSkin*>(mesh->GetDeformer(s, FbxDeformer::eSkin));
        int clustureCount = skin->GetClusterCount();
        for (int c = 0; c < clustureCount; ++c)
        {
            FbxCluster* cluster = skin->GetCluster(c);
            FbxNode* linkedBone = cluster->GetLink();
            if (!linkedBone)
                continue;

            FString boneName = linkedBone->GetName();
            int boneIndex = boneNameToIndex[boneName];

            int* indices = cluster->GetControlPointIndices();
            double* weights = cluster->GetControlPointWeights();
            int count = cluster->GetControlPointIndicesCount();
            for (int i = 0; i < count; ++i)
            {
                int ctrlIdx = indices[i];
                float weight = static_cast<float>(weights[i]);
                OutBoneWeights[ctrlIdx].Add({boneIndex, weight});
            }
            
        }
    } 
}

// 신규 함수: 애니메이션만 파싱
void FFbxLoader::ParseFBXAnimationOnly(const FString& filename, USkeletalMesh* skeletalMesh)
{
    FbxScene* scene = FbxScene::Create(Manager, "");
    FbxImporter* importer = FbxImporter::Create(Manager, "");

    if (!importer->Initialize(GetData(filename), -1, GetFbxIOSettings()))
    {
        importer->Destroy();
        return;
    }

    if (!importer->Import(scene))
    {
        importer->Destroy();
        return;
    }

    importer->Destroy();

    TArray<UAnimSequence*> Sequences;
    LoadAnimationInfo(scene, skeletalMesh, Sequences);
    for (UAnimSequence* Sequence : Sequences)
    {
        LoadAnimationData(scene, scene->GetRootNode(), skeletalMesh, Sequence);
    }

#ifdef DEBUG_DUMP_ANIMATION
    DumpAnimationDebug(filename, skeletalMesh, Sequences);
#endif

    scene->Destroy();
}

void FFbxLoader::LoadAnimationInfo(FbxScene* Scene, USkeletalMesh* SkeletalMesh, TArray<UAnimSequence*>& OutSequences)
{
    FbxArray<FbxString*> animNames;
    Scene->FillAnimStackNameArray(animNames);

    for (int i = 0; i < animNames.Size(); ++i)
    {
        const char* currentFbxStackName = animNames[i]->Buffer();
        FbxAnimStack* animStack = Scene->FindMember<FbxAnimStack>(currentFbxStackName); // 변수명 변경 (animStack -> fbxAnimStack)
        if (!animStack) {
            UE_LOG(ELogLevel::Warning, "Could not find AnimStack in scene: %s", currentFbxStackName);
            continue;
        }

        FbxTakeInfo* takeInfo = Scene->GetTakeInfo(animStack->GetName());
        if (!takeInfo) { continue; }

        UAnimSequence* Sequence = FObjectFactory::ConstructObject<UAnimSequence>(nullptr);
        Sequence->SetName(FString(animStack->GetName()));

        UAnimDataModel* DataModel = FObjectFactory::ConstructObject<UAnimDataModel>(Sequence);
        Sequence->SetDataModel(DataModel);

        const float Duration = (float)takeInfo->mLocalTimeSpan.GetDuration().GetSecondDouble();
        const FFrameRate FrameRate((int32)FbxTime::GetFrameRate(Scene->GetGlobalSettings().GetTimeMode()), 1);


        DataModel->PlayLength = Duration;
        DataModel->FrameRate = FrameRate;
        DataModel->NumberOfFrames = FMath::RoundToInt(Duration * FrameRate.Numerator / (float)FrameRate.Denominator);
        DataModel->NumberOfKeys = DataModel->NumberOfFrames;

        Sequence->SetSequenceLength( Duration);

        OutSequences.Add(Sequence);
    }

    for (int i = 0; i < animNames.Size(); ++i)
        delete animNames[i];
    animNames.Clear();

    UE_LOG(ELogLevel::Display, "Loaded %d animation sequence(s).", OutSequences.Num());
    for (UAnimSequence* Seq : OutSequences)
    {
        UE_LOG(ELogLevel::Display, "  → Sequence: %s, Duration: %.2f, Frames: %d",
            *Seq->GetSeqName(),
            Seq->GetDataModel()->PlayLength,
            Seq->GetDataModel()->NumberOfFrames);
    }
}


void FFbxLoader::LoadAnimationData(FbxScene* Scene, FbxNode* RootNode, USkeletalMesh* SkeletalMesh, UAnimSequence* Sequence)
{
     if (!Sequence || !Sequence->GetDataModel() || !SkeletalMesh)
         return;

     auto NameDebug = *Sequence->GetSeqName();
     FbxAnimStack* AnimStack = Scene->FindMember<FbxAnimStack>(*Sequence->GetSeqName());
     if (!AnimStack)
     {
         UE_LOG(ELogLevel::Warning, "AnimStack not found for sequence: %s", *Sequence->GetSeqName());
         return;
     }

     Scene->SetCurrentAnimationStack(AnimStack);
     FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>();
     if (!AnimLayer) return;

     const int32 FrameCount = Sequence->GetDataModel()->NumberOfFrames;
     const float DeltaTime = Sequence->GetDataModel()->PlayLength / FrameCount;

     TArray<FBoneAnimationTrack>& Tracks = Sequence->GetDataModel()->BoneAnimationTracks;
    FReferenceSkeleton RefSkeleton;
    SkeletalMesh->GetRefSkeleton(RefSkeleton);

    Tracks.SetNum(RefSkeleton.RawRefBoneInfo.Num());

    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.RawRefBoneInfo.Num(); ++BoneIndex)
    {
        const FName& BoneName = RefSkeleton.RawRefBoneInfo[BoneIndex].Name;
        FbxNode* BoneNode = FindBoneNode(RootNode, BoneName.ToString());
        if (!BoneNode)
        {
            UE_LOG(ELogLevel::Warning, "BoneNode not found in FBX for bone: %s", *BoneName.ToString());
            continue;
        }

        FBoneAnimationTrack Track;
        Track.BoneTreeIndex = BoneIndex;
        Track.Name = BoneName;

        FRawAnimSequenceTrack& Raw = Track.InternalTrackData;
        Raw.PosKeys.SetNum(FrameCount);
        Raw.RotKeys.SetNum(FrameCount);
        Raw.ScaleKeys.SetNum(FrameCount);

        for (int32 FrameIdx = 0; FrameIdx < FrameCount; ++FrameIdx)
        {
            FbxTime Time;
            Time.SetSecondDouble(FrameIdx * DeltaTime);

            FbxAMatrix LocalMatrix = BoneNode->EvaluateLocalTransform(Time);
            FTransform Transform = FTransformFromFbxMatrix(LocalMatrix);

            Raw.PosKeys[FrameIdx] = Transform.Translation;
            Raw.RotKeys[FrameIdx] = Transform.Rotation.Quaternion();
            Raw.ScaleKeys[FrameIdx] = Transform.Scale3D;
        }

        Tracks[BoneIndex] = std::move(Track);
    }
}

void FFbxLoader::DumpAnimationDebug(const FString& FBXFilePath, const USkeletalMesh* SkeletalMesh, const TArray<UAnimSequence*>& AnimSequences)
{
    FString DebugFilename = FBXFilePath + "_debug.txt";
    std::ofstream File(*DebugFilename);

    if (!File.is_open())
    {
        UE_LOG(ELogLevel::Error, "Cannot open debug file: %s", *DebugFilename);
        return;
    }

    for (const UAnimSequence* Seq : AnimSequences)
    {
        File << "[Sequence] " << (*Seq->GetName()) << "\n";
        File << "Duration: " << Seq->GetDataModel()->PlayLength << " seconds\n";
        File << "FrameRate: " << Seq->GetDataModel()->FrameRate.Numerator << "\n";
        File << "Frames: " << Seq->GetDataModel()->NumberOfFrames << "\n\n";

        const TArray<FBoneAnimationTrack>& Tracks = Seq->GetDataModel()->BoneAnimationTracks;

        for (const FBoneAnimationTrack& Track : Tracks)
        {
            File << "Bone [" << (*Track.Name.ToString()) << "] Index: " << Track.BoneTreeIndex << "\n";
            const FRawAnimSequenceTrack& Raw = Track.InternalTrackData;

            for (int32 i = 0; i < Raw.PosKeys.Num(); ++i)
            {
                const FVector& T = Raw.PosKeys[i];
                const FQuat& R = Raw.RotKeys[i];
                const FVector& S = Raw.ScaleKeys[i];

                File << "  Frame " << i << ":\n";
                File << "    Pos: (" << T.X << ", " << T.Y << ", " << T.Z << ")\n";
                File << "    Rot: (" << R.X << ", " << R.Y << ", " << R.Z << ", " << R.W << ")\n";
                File << "    Scale: (" << S.X << ", " << S.Y << ", " << S.Z << ")\n";
            }

            File << "\n";
        }
    }

    File.close();
}

void FFbxLoader::LoadFBXMesh(
    FFbxSkeletalMesh* fbxObject,
    FbxNode* node,
    TMap<FString, int>& boneNameToIndex,
    TMap<int, TArray<BoneWeights>>& boneWeight
)
{
    FbxMesh* mesh = node->GetMesh();
    if (!mesh)
        return;

    FFbxMeshData meshData;
    meshData.name = node->GetName();
    
    int polygonCount = mesh->GetPolygonCount();
    FbxVector4* controlPoints = mesh->GetControlPoints();
    FbxLayerElementNormal* normalElement = mesh->GetElementNormal();
    FbxLayerElementTangent* tangentElement = mesh->GetElementTangent();
    FbxLayerElementUV* uvElement = mesh->GetElementUV();
    FbxLayerElementMaterial* materialElement = mesh->GetElementMaterial();

    FVector AABBmin(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector AABBmax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    TMap<FString, uint32> indexMap;
    TMap<int, TArray<uint32>> materialIndexToIndices;
    
    for (int polygonIndex = 0; polygonIndex < polygonCount; ++polygonIndex)
    {
        for (int vertexIndex = 0; vertexIndex < 3; ++vertexIndex)
        {
            FFbxVertex v;
            int controlPointIndex = mesh->GetPolygonVertex(polygonIndex, vertexIndex);
            int polygonVertexIndex = polygonIndex * 3 + vertexIndex;
            
            // vertex
            FbxVector4 pos = controlPoints[controlPointIndex];
            FVector convertPos(pos[0], pos[1], pos[2]);
            v.position = convertPos;
            
            AABBmin.X = std::min(AABBmin.X, v.position.X);
            AABBmin.Y = std::min(AABBmin.Y, v.position.Y);
            AABBmin.Z = std::min(AABBmin.Z, v.position.Z);
            AABBmax.X = std::max(AABBmax.X, v.position.X);
            AABBmax.Y = std::max(AABBmax.Y, v.position.Y);
            AABBmax.Z = std::max(AABBmax.Z, v.position.Z);
            
            // Normal
            FbxVector4 normal = {0, 0, 0};
            if (normalElement) {
                int index;
                switch (normalElement->GetMappingMode())
                {
                case FbxLayerElement::eByControlPoint:
                    index = controlPointIndex;
                    break;
                case FbxLayerElement::eByPolygonVertex:
                    index = polygonVertexIndex;
                    break;
                case FbxLayerElement::eByPolygon:
                    index = polygonIndex;
                    break;
                default:
                    index = 0;
                    break;
                }
                int normIdx = (normalElement->GetReferenceMode() == FbxLayerElement::eDirect)
                            ? index
                            : normalElement->GetIndexArray().GetAt(index);
                normal = normalElement->GetDirectArray().GetAt(normIdx);
            }
            FVector convertNormal(normal[0], normal[1], normal[2]);
            v.normal = convertNormal;

            // Tangent
            FbxVector4 tangent = {0, 0, 0};
            if (tangentElement)
            {
                int index;
                switch (tangentElement->GetMappingMode())
                {
                case FbxLayerElement::eByControlPoint:
                    index = controlPointIndex;
                    break;
                case FbxLayerElement::eByPolygonVertex:
                    index = polygonVertexIndex;
                    break;
                case FbxLayerElement::eByPolygon:
                    index = polygonIndex;
                    break;
                default:
                    index = 0;
                    break;
                }
                int tangentIdx = (tangentElement->GetReferenceMode() == FbxLayerElement::eDirect)
                                ? index
                                : tangentElement->GetIndexArray().GetAt(index);
                tangent = tangentElement->GetDirectArray().GetAt(tangentIdx);
            }
            FVector convertTangent = FVector(tangent[0], tangent[1], tangent[2]);
            v.tangent = convertTangent;

            // UV
            FbxVector2 uv = {0, 0};
            if (uvElement) {
                int uvIdx = mesh->GetTextureUVIndex(polygonIndex, vertexIndex);
                uv = uvElement->GetDirectArray().GetAt(uvIdx);
            }
            FVector2D convertUV(uv[0], 1.f - uv[1]);
            v.uv = convertUV;

            // Material & Subset
            if (materialElement)
            {
                const FbxLayerElementArrayTemplate<int>& indices = materialElement->GetIndexArray();
                v.materialIndex = indices.GetAt(polygonIndex);
                materialIndexToIndices[v.materialIndex].Add(static_cast<uint32>(controlPointIndex));
            }
            
            // Skin
            TArray<BoneWeights>* weights = boneWeight.Find(controlPointIndex);
            if (weights)
            {
                std::sort(weights->begin(), weights->end(), [](auto& a, auto& b)
                {
                    return a.weight > b.weight;
                });

                float total = 0.0f;
                for (int i = 0; i < 8 && i < weights->Num(); ++i)
                {
                    v.boneIndices[i] = (*weights)[i].jointIndex;
                    v.boneWeights[i] = (*weights)[i].weight;
                    total += (*weights)[i].weight;
                }


                // Normalize
                if (total > 0.f)
                {
                    for (int i = 0; i < 8; ++i)
                        v.boneWeights[i] /= total;
                }
            }

            // indices process
            std::stringstream ss;
            ss << GetData(convertPos.ToString()) << '|' << GetData(convertNormal.ToString()) << '|' << GetData(convertUV.ToString());
            FString key = ss.str();
            uint32 index;
            if (!indexMap.Contains(key))
            {
                index = meshData.vertices.Num();
                meshData.vertices.Add(v);
                indexMap[key] = index;
            } else
            {
                index = indexMap[key];
            }
            meshData.indices.Add(index);
        }
    }

    // subset 처리.
    uint32 accumIndex = 0;
    uint32 materialIndexOffset = fbxObject->material.Num();
    for (auto& [materialIndex, indices]: materialIndexToIndices)
    {
        FMaterialSubset subset;
        subset.IndexStart = static_cast<uint32>(accumIndex);
        subset.IndexCount = static_cast<uint32>(indices.Num());
        subset.MaterialIndex = materialIndexOffset + materialIndex;
        if (materialIndex < node->GetMaterialCount())
        {
            FbxSurfaceMaterial* mat = node->GetMaterial(materialIndex);
            if (mat)
                subset.MaterialName = mat->GetName();
        }
        accumIndex += indices.Num();
        meshData.subsetIndex.Add(fbxObject->materialSubsets.Num());
        fbxObject->materialSubsets.Add(subset);
    }

    // tangent 없을 경우 처리.
    if (tangentElement == nullptr)
    {
        for (int i = 0; i + 2 < meshData.indices.Num(); i += 3)
        {
            FFbxVertex& Vertex0 = meshData.vertices[meshData.indices[i]];
            FFbxVertex& Vertex1 = meshData.vertices[meshData.indices[i + 1]];
            FFbxVertex& Vertex2 = meshData.vertices[meshData.indices[i + 2]];

            CalculateTangent(Vertex0, Vertex1, Vertex2);
            CalculateTangent(Vertex1, Vertex2, Vertex0);
            CalculateTangent(Vertex2, Vertex0, Vertex1);
        }
        
    }

    // AABB 설정.
    fbxObject->AABBmin = AABBmin;
    fbxObject->AABBmax = AABBmax;

    fbxObject->mesh.Add(meshData);
}


void FFbxLoader::LoadFBXMaterials(
    FFbxSkeletalMesh* fbxObject,
    FbxNode* node
)
{
    if (!node)
        return;

    int materialCount = node->GetMaterialCount();
    
    for (int i = 0; i < materialCount; ++i)
    {
        FbxSurfaceMaterial* material = node->GetMaterial(i);

        UMaterial* materialInfo = FObjectFactory::ConstructObject<UMaterial>(nullptr);
        
        materialInfo->GetMaterialInfo().MaterialName = material->GetName();
        int reservedCount = static_cast<uint32>(EMaterialTextureSlots::MTS_MAX);
        for (int i = 0; i < reservedCount; ++i)
            materialInfo->GetMaterialInfo().TextureInfos.Add({});
        
        // normalMap
        FbxProperty normal = material->FindProperty(FbxSurfaceMaterial::sNormalMap);
        if (normal.IsValid())
        {
            FbxTexture* texture = normal.GetSrcObject<FbxTexture>();
            FbxFileTexture* fileTexture = FbxCast<FbxFileTexture>(texture);
            if (fileTexture && CreateTextureFromFile(StringToWString(fileTexture->GetFileName()), false))
            {
                const uint32 SlotIdx = static_cast<uint32>(EMaterialTextureSlots::MTS_Normal);
                FTextureInfo& slot = materialInfo->GetMaterialInfo().TextureInfos[SlotIdx];
                slot.TextureName = fileTexture->GetName();
                slot.TexturePath = StringToWString(fileTexture->GetFileName());
                slot.bIsSRGB = false;
                materialInfo->GetMaterialInfo().TextureFlag |= static_cast<uint16>(EMaterialTextureFlags::MTF_Normal);
            }
        }
        
        // diffuse
        FbxProperty diffuse = material->FindProperty(FbxSurfaceMaterial::sDiffuse);
        if (diffuse.IsValid())
        {
            FbxDouble3 color = diffuse.Get<FbxDouble3>();
            materialInfo->SetDiffuse(FVector(color[0], color[1], color[2]));
            
            FbxTexture* texture = diffuse.GetSrcObject<FbxTexture>();
            FbxFileTexture* fileTexture = FbxCast<FbxFileTexture>(texture);
            if (fileTexture && CreateTextureFromFile(StringToWString(fileTexture->GetFileName()), true))
            {
                const uint32 SlotIdx = static_cast<uint32>(EMaterialTextureSlots::MTS_Diffuse);
                FTextureInfo& slot = materialInfo->GetMaterialInfo().TextureInfos[SlotIdx];
                slot.TextureName = fileTexture->GetName();
                slot.TexturePath = StringToWString(fileTexture->GetFileName());
                slot.bIsSRGB = true;
                materialInfo->GetMaterialInfo().TextureFlag |= static_cast<uint16>(EMaterialTextureFlags::MTF_Diffuse);
            }
        }
        
        // ambient
        FbxProperty ambient = material->FindProperty(FbxSurfaceMaterial::sAmbient);
        if (ambient.IsValid())
        {
            FbxDouble3 color = ambient.Get<FbxDouble3>();
            materialInfo->SetAmbient(FVector(color[0], color[1], color[2]));
            
            FbxTexture* texture = ambient.GetSrcObject<FbxTexture>();
            FbxFileTexture* fileTexture = FbxCast<FbxFileTexture>(texture);
            if (fileTexture && CreateTextureFromFile(StringToWString(fileTexture->GetFileName()), true))
            {
                const uint32 SlotIdx = static_cast<uint32>(EMaterialTextureSlots::MTS_Ambient);
                FTextureInfo& slot = materialInfo->GetMaterialInfo().TextureInfos[SlotIdx];
                slot.TextureName = fileTexture->GetName();
                slot.TexturePath = StringToWString(fileTexture->GetFileName());
                slot.bIsSRGB = true;
                materialInfo->GetMaterialInfo().TextureFlag |= static_cast<uint16>(EMaterialTextureFlags::MTF_Ambient);
            }

        }

        // specular
        FbxProperty specular = material->FindProperty(FbxSurfaceMaterial::sSpecular);
        if (ambient.IsValid())
        {
            FbxDouble3 color = specular.Get<FbxDouble3>();
            materialInfo->SetSpecular(FVector(color[0], color[1], color[2]));
            
            FbxTexture* texture = specular.GetSrcObject<FbxTexture>();
            FbxFileTexture* fileTexture = FbxCast<FbxFileTexture>(texture);
            if (fileTexture && CreateTextureFromFile(StringToWString(fileTexture->GetFileName()), true))
            {
                const uint32 SlotIdx = static_cast<uint32>(EMaterialTextureSlots::MTS_Specular);
                FTextureInfo& slot = materialInfo->GetMaterialInfo().TextureInfos[SlotIdx];
                slot.TextureName = fileTexture->GetName();
                slot.TexturePath = StringToWString(fileTexture->GetFileName());
                slot.bIsSRGB = true;
                materialInfo->GetMaterialInfo().TextureFlag |= static_cast<uint16>(EMaterialTextureFlags::MTF_Specular);
            }
        }

        // emissive
        FbxProperty emissive = material->FindProperty(FbxSurfaceMaterial::sEmissive);
        FbxProperty emissiveFactor = material->FindProperty(FbxSurfaceMaterial::sEmissiveFactor);
        if (ambient.IsValid())
        {
            FbxDouble3 color = emissive.Get<FbxDouble3>();
            double intensity = emissiveFactor.Get<FbxDouble>();
            materialInfo->SetEmissive(FVector(color[0], color[1], color[2]), intensity);
            
            FbxTexture* texture = emissive.GetSrcObject<FbxTexture>();
            FbxFileTexture* fileTexture = FbxCast<FbxFileTexture>(texture);
            if (fileTexture && CreateTextureFromFile(StringToWString(fileTexture->GetFileName()), true))
            {
                const uint32 SlotIdx = static_cast<uint32>(EMaterialTextureSlots::MTS_Emissive);
                FTextureInfo& slot = materialInfo->GetMaterialInfo().TextureInfos[SlotIdx];
                slot.TextureName = fileTexture->GetName();
                slot.TexturePath = StringToWString(fileTexture->GetFileName());
                slot.bIsSRGB = true;
                
                materialInfo->GetMaterialInfo().TextureFlag |= static_cast<uint16>(EMaterialTextureFlags::MTF_Emissive);
            }
        }
        
        fbxObject->material.Add(materialInfo);
    }
}

bool FFbxLoader::CreateTextureFromFile(const FWString& Filename, bool bIsSRGB)
{
    if (FEngineLoop::ResourceManager.GetTexture(Filename))
    {
        return true;
    }

    HRESULT hr = FEngineLoop::ResourceManager.LoadTextureFromFile(FEngineLoop::GraphicDevice.Device, Filename.c_str(), bIsSRGB);

    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

void FFbxLoader::CalculateTangent(FFbxVertex& PivotVertex, const FFbxVertex& Vertex1, const FFbxVertex& Vertex2)
{
    const float s1 = Vertex1.uv.X - PivotVertex.uv.X;
    const float t1 = Vertex1.uv.Y - PivotVertex.uv.Y;
    const float s2 = Vertex2.uv.X - PivotVertex.uv.X;
    const float t2 = Vertex2.uv.Y - PivotVertex.uv.Y;
    const float E1x = Vertex1.position.X - PivotVertex.position.X;
    const float E1y = Vertex1.position.Y - PivotVertex.position.Y;
    const float E1z = Vertex1.position.Z - PivotVertex.position.Z;
    const float E2x = Vertex2.position.X - PivotVertex.position.X;
    const float E2y = Vertex2.position.Y - PivotVertex.position.Y;
    const float E2z = Vertex2.position.Z - PivotVertex.position.Z;

    const float Denominator = s1 * t2 - s2 * t1;
    FVector Tangent(1, 0, 0);
    FVector BiTangent(0, 1, 0);
    FVector Normal(PivotVertex.normal.X, PivotVertex.normal.Y, PivotVertex.normal.Z);
    
    if (FMath::Abs(Denominator) > SMALL_NUMBER)
    {
        // 정상적인 계산 진행
        const float f = 1.f / Denominator;
        
        const float Tx = f * (t2 * E1x - t1 * E2x);
        const float Ty = f * (t2 * E1y - t1 * E2y);
        const float Tz = f * (t2 * E1z - t1 * E2z);
        Tangent = FVector(Tx, Ty, Tz).GetSafeNormal();

        const float Bx = f * (-s2 * E1x + s1 * E2x);
        const float By = f * (-s2 * E1y + s1 * E2y);
        const float Bz = f * (-s2 * E1z + s1 * E2z);
        BiTangent = FVector(Bx, By, Bz).GetSafeNormal();
    }
    else
    {
        // 대체 탄젠트 계산 방법
        // 방법 1: 다른 방향에서 탄젠트 계산 시도
        FVector Edge1(E1x, E1y, E1z);
        FVector Edge2(E2x, E2y, E2z);
    
        // 기하학적 접근: 두 에지 사이의 각도 이등분선 사용
        Tangent = (Edge1.GetSafeNormal() + Edge2.GetSafeNormal()).GetSafeNormal();
    
        // 만약 두 에지가 평행하거나 반대 방향이면 다른 방법 사용
        if (Tangent.IsNearlyZero())
        {
            // TODO: 기본 축 방향 중 하나 선택 (메시의 주 방향에 따라 선택)
            Tangent = FVector(1.0f, 0.0f, 0.0f);
        }
    }

    Tangent = (Tangent - Normal * FVector::DotProduct(Normal, Tangent)).GetSafeNormal();
    
    const float Sign = (FVector::DotProduct(FVector::CrossProduct(Normal, Tangent), BiTangent) < 0.f) ? -1.f : 1.f;

    PivotVertex.tangent.X = Tangent.X;
    PivotVertex.tangent.Y = Tangent.Y;
    PivotVertex.tangent.Z = Tangent.Z;
    PivotVertex.tangent.W = Sign;
}

// .bin 파일로 저장합니다.
bool FFbxManager::SaveFBXToBinary(const FWString& FilePath, int64_t LastModifiedTime, const FFbxSkeletalMesh& FBXObject)
{
    /** File Open */
    std::ofstream File(FilePath, std::ios::binary);

    if (!File.is_open())
    {
        assert("CAN'T SAVE FBX FILE TO BINARY");
        return false;
    }

    /** Modified */
    File.write(reinterpret_cast<const char*>(&LastModifiedTime), sizeof(&LastModifiedTime));

    /** FBX Name */
    Serializer::WriteFString(File, FBXObject.name);

    /** FBX Mesh */
    uint32 MeshCount = FBXObject.mesh.Num();
    File.write(reinterpret_cast<const char*>(&MeshCount), sizeof(MeshCount));
    for (const FFbxMeshData& MeshData : FBXObject.mesh)
    {
        // Mesh Vertices
        uint32 VertexCount = MeshData.vertices.Num();
        File.write(reinterpret_cast<const char*>(&VertexCount), sizeof(VertexCount));
        if (VertexCount > 0)
        {
            File.write(reinterpret_cast<const char*>(MeshData.vertices.GetData()), sizeof(FFbxVertex) * VertexCount);
        }

        // Mesh Indices
        uint32 IndexCount = MeshData.indices.Num();
        File.write(reinterpret_cast<const char*>(&IndexCount), sizeof(IndexCount));
        if (IndexCount > 0)
        {
            File.write(reinterpret_cast<const char*>(MeshData.indices.GetData()), sizeof(uint32) * IndexCount);
        }

        // Subset
        uint32 SubIndexCount = MeshData.subsetIndex.Num();
        File.write(reinterpret_cast<const char*>(&SubIndexCount), sizeof(SubIndexCount));
        if (SubIndexCount > 0)
        {
            File.write(reinterpret_cast<const char*>(MeshData.subsetIndex.GetData()), sizeof(uint32) * SubIndexCount);
        }

        // Name
        Serializer::WriteFString(File, MeshData.name);
    }
    
    /** FBX Skeleton */
    uint32 JointCount = FBXObject.skeleton.joints.Num();
    File.write(reinterpret_cast<const char*>(&JointCount), sizeof(JointCount));
    for (const FFbxJoint& Joint : FBXObject.skeleton.joints)
    {
        // Joint Name
        Serializer::WriteFString(File, Joint.name);

        // Parent index
        File.write(reinterpret_cast<const char*>(&Joint.parentIndex), sizeof(Joint.parentIndex));

        // Local bind pose
        File.write(reinterpret_cast<const char*>(&Joint.localBindPose), sizeof(Joint.localBindPose));

        // Inverse bind pose
        File.write(reinterpret_cast<const char*>(&Joint.inverseBindPose), sizeof(Joint.inverseBindPose));

        // Position
        File.write(reinterpret_cast<const char*>(&Joint.position), sizeof(Joint.position));

        // Rotation
        File.write(reinterpret_cast<const char*>(&Joint.rotation), sizeof(Joint.rotation));

        // Scale
        File.write(reinterpret_cast<const char*>(&Joint.scale), sizeof(Joint.scale));
    }

    /** FBX UMaterial */
    uint32 MaterialCount = FBXObject.material.Num();
    File.write(reinterpret_cast<const char*>(&MaterialCount), sizeof(MaterialCount));
    for (UMaterial* const Material : FBXObject.material)
    {
        bool bIsValidMaterial = (Material != nullptr);
        File.write(reinterpret_cast<const char*>(&bIsValidMaterial), sizeof(bIsValidMaterial));
        
        if (bIsValidMaterial)
        {
            const FMaterialInfo& MaterialInfo = Material->GetMaterialInfo();

            // MaterialInfo.MaterialName (FString)
            Serializer::WriteFString(File, MaterialInfo.MaterialName);
            
            // MaterialInfo.TextureFlag (uint32)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.TextureFlag), sizeof(MaterialInfo.TextureFlag));
            
            // MaterialInfo.bTransparent (bool)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.bTransparent), sizeof(MaterialInfo.bTransparent));
            
            // MaterialInfo.DiffuseColor (FVector)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.DiffuseColor), sizeof(MaterialInfo.DiffuseColor));
            
            // MaterialInfo.SpecularColor (FVector)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.SpecularColor), sizeof(MaterialInfo.SpecularColor));
            
            // MaterialInfo.AmbientColor (FVector)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.AmbientColor), sizeof(MaterialInfo.AmbientColor));
            
            // MaterialInfo.EmissiveColor (FVector)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.EmissiveColor), sizeof(MaterialInfo.EmissiveColor));
            
            // MaterialInfo.SpecularExponent (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.SpecularExponent), sizeof(MaterialInfo.SpecularExponent));
            
            // MaterialInfo.IOR (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.IOR), sizeof(MaterialInfo.IOR));
            
            // MaterialInfo.Transparency (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.Transparency), sizeof(MaterialInfo.Transparency));
            
            // MaterialInfo.BumpMultiplier (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.BumpMultiplier), sizeof(MaterialInfo.BumpMultiplier));
            
            // MaterialInfo.IlluminanceModel (uint32)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.IlluminanceModel), sizeof(MaterialInfo.IlluminanceModel));
            
            // MaterialInfo.Metallic (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.Metallic), sizeof(MaterialInfo.Metallic));
            
            // MaterialInfo.Roughness (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.Roughness), sizeof(MaterialInfo.Roughness));
            
            // MaterialInfo.AmbientOcclusion (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.AmbientOcclusion), sizeof(MaterialInfo.AmbientOcclusion));
            
            // MaterialInfo.ClearCoat (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.ClearCoat), sizeof(MaterialInfo.ClearCoat));
            
            // MaterialInfo.Sheen (float)
            File.write(reinterpret_cast<const char*>(&MaterialInfo.Sheen), sizeof(MaterialInfo.Sheen));

            // MaterialInfo.TextureInfos (TArray<FTextureInfo>)
            uint32 TextureInfoCount = MaterialInfo.TextureInfos.Num();
            File.write(reinterpret_cast<const char*>(&TextureInfoCount), sizeof(TextureInfoCount));
            for (const FTextureInfo& texInfo : MaterialInfo.TextureInfos)
            {
                Serializer::WriteFString(File, texInfo.TextureName);
                 Serializer::WriteFWString(File, texInfo.TexturePath);
                File.write(reinterpret_cast<const char*>(&texInfo.bIsSRGB), sizeof(texInfo.bIsSRGB));
            }
        }
    }

    /** FBX Material Subset */
    uint32 SubsetCount = FBXObject.materialSubsets.Num();
    File.write(reinterpret_cast<const char*>(&SubsetCount), sizeof(SubsetCount));
    for (const FMaterialSubset& Subset : FBXObject.materialSubsets)
    {
        File.write(reinterpret_cast<const char*>(&Subset.IndexStart), sizeof(Subset.IndexStart));
        File.write(reinterpret_cast<const char*>(&Subset.IndexCount), sizeof(Subset.IndexCount));
        File.write(reinterpret_cast<const char*>(&Subset.MaterialIndex), sizeof(Subset.MaterialIndex));
        Serializer::WriteFString(File, Subset.MaterialName);
    }
    
    /** FBX AABB */
    File.write(reinterpret_cast<const char*>(&FBXObject.AABBmin), sizeof(FVector));
    File.write(reinterpret_cast<const char*>(&FBXObject.AABBmax), sizeof(FVector));
    
    File.close();
    return true;
}

// .bin 파일을 파싱합니다.
bool FFbxManager::LoadFBXFromBinary(const FWString& FilePath, int64_t LastModifiedTime, FFbxSkeletalMesh& OutFBXObject)
{
    UE_LOG(ELogLevel::Display, "Start FBX Parsing : %s", WStringToString(FilePath).c_str());
    std::ifstream File(FilePath, std::ios::binary);
    if (!File.is_open())
    {
        assert("CAN'T OPEN FBX BINARY FILE");
        return false;
    }

    /** Modified Check */
    int64_t FileLastModifiedTime;
    File.read(reinterpret_cast<char*>(&FileLastModifiedTime), sizeof(FileLastModifiedTime));

    // File is changed.
    if (LastModifiedTime != FileLastModifiedTime)
    {
        return false;
    }

    TArray<TPair<FWString, bool>> Textures;

    /** FBX Name */
    Serializer::ReadFString(File, OutFBXObject.name);
    
    /** FBX Mesh */
    uint32 MeshCount;
    File.read(reinterpret_cast<char*>(&MeshCount), sizeof(MeshCount));
    OutFBXObject.mesh.Reserve(MeshCount); // 미리 메모리 할당
    for (uint32 i = 0; i < MeshCount; ++i)
    {
        FFbxMeshData MeshData;

        // Mesh Vertices
        uint32 VertexCount;
        File.read(reinterpret_cast<char*>(&VertexCount), sizeof(VertexCount));
        if (VertexCount > 0)
        {
            MeshData.vertices.SetNum(VertexCount); // 크기 설정
            File.read(reinterpret_cast<char*>(MeshData.vertices.GetData()), sizeof(FFbxVertex) * VertexCount);
        }

        // Mesh Indices
        uint32 IndexCount;
        File.read(reinterpret_cast<char*>(&IndexCount), sizeof(IndexCount));
        if (IndexCount > 0)
        {
            MeshData.indices.SetNum(IndexCount); // 크기 설정
            File.read(reinterpret_cast<char*>(MeshData.indices.GetData()), sizeof(uint32) * IndexCount);
        }
        
        // Subset
        uint32 SubIndexCount;
        File.read(reinterpret_cast<char*>(&SubIndexCount), sizeof(SubIndexCount));
        if (SubIndexCount > 0)
        {
            MeshData.subsetIndex.SetNum(SubIndexCount); // 크기 설정
            File.read(reinterpret_cast<char*>(MeshData.subsetIndex.GetData()), sizeof(uint32) * SubIndexCount);
        }

        // Name
        Serializer::ReadFString(File, MeshData.name);
        OutFBXObject.mesh.Add(std::move(MeshData));
    }

    /** FBX Skeleton */
    uint32 JointCount;
    File.read(reinterpret_cast<char*>(&JointCount), sizeof(JointCount));
    OutFBXObject.skeleton.joints.Reserve(JointCount); // 미리 메모리 할당
    for (uint32 i = 0; i < JointCount; ++i)
    {
        FFbxJoint Joint;

        // Joint Name
        Serializer::ReadFString(File, Joint.name);

        // Parent index
        File.read(reinterpret_cast<char*>(&Joint.parentIndex), sizeof(Joint.parentIndex));

        // Local bind pose
        File.read(reinterpret_cast<char*>(&Joint.localBindPose), sizeof(Joint.localBindPose));

        // Inverse bind pose
        File.read(reinterpret_cast<char*>(&Joint.inverseBindPose), sizeof(Joint.inverseBindPose));

        // Position
        File.read(reinterpret_cast<char*>(&Joint.position), sizeof(Joint.position));

        // Rotation
        File.read(reinterpret_cast<char*>(&Joint.rotation), sizeof(Joint.rotation));

        // Scale
        File.read(reinterpret_cast<char*>(&Joint.scale), sizeof(Joint.scale));
        
        OutFBXObject.skeleton.joints.Add(std::move(Joint));
    }
    
    /** FBX UMaterial */
    uint32 MaterialCount;
    File.read(reinterpret_cast<char*>(&MaterialCount), sizeof(MaterialCount));
    OutFBXObject.material.Reserve(MaterialCount); // 미리 메모리 할당
    for (uint32 i = 0; i < MaterialCount; ++i)
    {
        bool bIsValidMaterial;
        File.read(reinterpret_cast<char*>(&bIsValidMaterial), sizeof(bIsValidMaterial));
        
        if (bIsValidMaterial)
        {
            UMaterial* NewMaterial = new UMaterial(); // UMaterial 객체 생성
            FMaterialInfo MaterialInfo;

            // MaterialInfo.MaterialName (FString)
            Serializer::ReadFString(File, MaterialInfo.MaterialName);
            
            // MaterialInfo.TextureFlag (uint32)
            File.read(reinterpret_cast<char*>(&MaterialInfo.TextureFlag), sizeof(MaterialInfo.TextureFlag));
            
            // MaterialInfo.bTransparent (bool)
            File.read(reinterpret_cast<char*>(&MaterialInfo.bTransparent), sizeof(MaterialInfo.bTransparent));
            
            // MaterialInfo.DiffuseColor (FVector)
            File.read(reinterpret_cast<char*>(&MaterialInfo.DiffuseColor), sizeof(MaterialInfo.DiffuseColor));
            
            // MaterialInfo.SpecularColor (FVector)
            File.read(reinterpret_cast<char*>(&MaterialInfo.SpecularColor), sizeof(MaterialInfo.SpecularColor));
            
            // MaterialInfo.AmbientColor (FVector)
            File.read(reinterpret_cast<char*>(&MaterialInfo.AmbientColor), sizeof(MaterialInfo.AmbientColor));
            
            // MaterialInfo.EmissiveColor (FVector)
            File.read(reinterpret_cast<char*>(&MaterialInfo.EmissiveColor), sizeof(MaterialInfo.EmissiveColor));
            
            // MaterialInfo.SpecularExponent (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.SpecularExponent), sizeof(MaterialInfo.SpecularExponent));
            
            // MaterialInfo.IOR (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.IOR), sizeof(MaterialInfo.IOR));
            
            // MaterialInfo.Transparency (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.Transparency), sizeof(MaterialInfo.Transparency));
            
            // MaterialInfo.BumpMultiplier (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.BumpMultiplier), sizeof(MaterialInfo.BumpMultiplier));
            
            // MaterialInfo.IlluminanceModel (uint32)
            File.read(reinterpret_cast<char*>(&MaterialInfo.IlluminanceModel), sizeof(MaterialInfo.IlluminanceModel));
            
            // MaterialInfo.Metallic (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.Metallic), sizeof(MaterialInfo.Metallic));
            
            // MaterialInfo.Roughness (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.Roughness), sizeof(MaterialInfo.Roughness));
            
            // MaterialInfo.AmbientOcclusion (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.AmbientOcclusion), sizeof(MaterialInfo.AmbientOcclusion));
            
            // MaterialInfo.ClearCoat (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.ClearCoat), sizeof(MaterialInfo.ClearCoat));
            
            // MaterialInfo.Sheen (float)
            File.read(reinterpret_cast<char*>(&MaterialInfo.Sheen), sizeof(MaterialInfo.Sheen));

            // MaterialInfo.TextureInfos (TArray<FTextureInfo>)
            uint32 TextureInfoCount;
            File.read(reinterpret_cast<char*>(&TextureInfoCount), sizeof(TextureInfoCount));
            MaterialInfo.TextureInfos.Reserve(TextureInfoCount); // 미리 메모리 할당
            for (uint32 j = 0; j < TextureInfoCount; ++j)
            {
                FTextureInfo TexInfo;
                Serializer::ReadFString(File, TexInfo.TextureName);
                Serializer::ReadFWString(File, TexInfo.TexturePath);
                File.read(reinterpret_cast<char*>(&TexInfo.bIsSRGB), sizeof(TexInfo.bIsSRGB));
                Textures.AddUnique({TexInfo.TexturePath, TexInfo.bIsSRGB});
                MaterialInfo.TextureInfos.Add(std::move(TexInfo));
            }
            NewMaterial->SetMaterialInfo(MaterialInfo);
            OutFBXObject.material.Add(NewMaterial);
        }
        else
        {
            OutFBXObject.material.Add(nullptr);
        }
    }

    /** FBX Material Subset */
    uint32 SubsetCount;
    File.read(reinterpret_cast<char*>(&SubsetCount), sizeof(SubsetCount));
    OutFBXObject.materialSubsets.Reserve(SubsetCount); // 미리 메모리 할당
    for (uint32 i = 0; i < SubsetCount; ++i)
    {
        FMaterialSubset Subset;
        File.read(reinterpret_cast<char*>(&Subset.IndexStart), sizeof(Subset.IndexStart));
        File.read(reinterpret_cast<char*>(&Subset.IndexCount), sizeof(Subset.IndexCount));
        File.read(reinterpret_cast<char*>(&Subset.MaterialIndex), sizeof(Subset.MaterialIndex));
        Serializer::ReadFString(File, Subset.MaterialName);
        OutFBXObject.materialSubsets.Add(std::move(Subset));
    }
    
    /** FBX AABB */
    File.read(reinterpret_cast<char*>(&OutFBXObject.AABBmin), sizeof(FVector));
    File.read(reinterpret_cast<char*>(&OutFBXObject.AABBmax), sizeof(FVector));
    
    File.close();

    // Texture load
    if (Textures.Num() > 0)
    {
        for (const TPair<FWString, bool>& Texture : Textures)
        {
            if (FEngineLoop::ResourceManager.GetTexture(Texture.Key) == nullptr)
            {
                FEngineLoop::ResourceManager.LoadTextureFromFile(FEngineLoop::GraphicDevice.Device, Texture.Key.c_str(), Texture.Value);
            }
        }
    }
    
    return true;
}


FbxNode* FFbxLoader::FindBoneNode(FbxNode* Root, const FString& BoneName)
{
    if (!Root) return nullptr;
    if (BoneName.Equals(Root->GetName(), ESearchCase::IgnoreCase)) return Root;

    for (int32 i = 0; i < Root->GetChildCount(); ++i)
    {
        if (FbxNode* Found = FindBoneNode(Root->GetChild(i), BoneName))
            return Found;
    }
    return nullptr;
}

FTransform FFbxLoader::FTransformFromFbxMatrix(const FbxAMatrix& Matrix)
{
    FbxVector4 T = Matrix.GetT();
    FbxQuaternion Q = Matrix.GetQ();
    FbxVector4 S = Matrix.GetS();

    FVector Translation((float)T[0], (float)T[1], (float)T[2]);
    FVector Scale((float)S[0], (float)S[1], (float)S[2]);
    FQuat Rotation((float)Q[0], (float)Q[1], (float)Q[2], (float)Q[3]);

    return FTransform(Translation, Rotation, Scale);
}
