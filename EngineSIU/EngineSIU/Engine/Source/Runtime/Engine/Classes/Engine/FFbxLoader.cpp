#include "FFbxLoader.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include "FBXObject.h"
#include "Serializer.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimData/AnimDataModel.h"
#include "UObject/ObjectFactory.h"
#include "Components/Material/Material.h"
#include "Engine/Asset/SkeletalMeshAsset.h"
#include "Components/Mesh/SkeletalMesh.h"
#include "Container/StringConv.h"

// 헬퍼 함수
namespace
{
    // 헬퍼 함수
// FBX 씬에서 joint name과 일치하는 FbxNode*를 찾아 반환
    FbxNode* FindFbxNodeByName(FbxNode* RootNode, const FString& TargetName)
    {
        if (!RootNode) return nullptr;

        if (TargetName == RootNode->GetName())
            return RootNode;

        const int ChildCount = RootNode->GetChildCount();
        for (int i = 0; i < ChildCount; ++i)
        {
            FbxNode* Result = FindFbxNodeByName(RootNode->GetChild(i), TargetName);
            if (Result)
                return Result;
        }
        return nullptr;
    }

    // FFbxSkeletonData의 joints 순서대로 FBX Node를 수집
    void CollectAllBoneNodes(FbxNode* RootNode, const FFbxSkeletonData& Skeleton, TArray<FbxNode*>& OutBoneNodes)
    {
        OutBoneNodes.Empty();
        for (const FFbxJoint& Joint : Skeleton.joints)
        {
            FbxNode* BoneNode = FindFbxNodeByName(RootNode, Joint.name);
            if (BoneNode)
            {
                OutBoneNodes.Add(BoneNode);
            }
            else
            {
                // 만약 못 찾으면 경고 로그
                UE_LOG(ELogLevel::Warning, TEXT("FBX Bone Node not found: %s"), *Joint.name);
                OutBoneNodes.Add(nullptr); // 인덱스 맞춤용
            }
        }
    }
};

//#define DEBUG_DUMP_ANIMATION

const FbxAxisSystem FFbxLoader::UnrealTargetAxisSystem(
    FbxAxisSystem::eZAxis,
    FbxAxisSystem::eParityEven,
    FbxAxisSystem::eRightHanded);

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

// .fbx -> FFbxSkeletalMesh
bool FFbxLoader::ParseFBX(const FString& FBXFilePath, 
    FFbxSkeletalMesh* OutFbxSkeletalMesh, 
    TArray<FFbxAnimSequence*>& OutFbxSequences,
    TArray<FFbxAnimStack*>& OutAnimStacks
)
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
        UE_LOG(ELogLevel::Warning, "Failed to parse FBX: %s", *FBXFilePath);
        return false;
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
        UE_LOG(ELogLevel::Warning, "Failed to parse FBX: %s", *FBXFilePath);
        return false;
    }
    converter = new FbxGeometryConverter(Manager);
    converter->Triangulate(scene, true);
    delete converter;

    // ✨ --- 좌표계 및 단위 변환 시작 ---
    FbxGlobalSettings& settings = scene->GetGlobalSettings();
    FbxAxisSystem currentAxisSystem = settings.GetAxisSystem();
    int sign = 0;
    UE_LOG(ELogLevel::Display, "Original FBX Axis System: Up(%d), Front(%d), Coord(%d)",
        currentAxisSystem.GetUpVector(sign),
        currentAxisSystem.GetFrontVector(sign), // GetFrontVector는 Parity를 반환
        currentAxisSystem.GetCoorSystem());

    // convert scene
    FbxAxisSystem sceneAxisSystem = scene->GetGlobalSettings().GetAxisSystem();

    if (sceneAxisSystem != UnrealTargetAxisSystem)
    {
        UnrealTargetAxisSystem.DeepConvertScene(scene);
    }
    
    FbxSystemUnit SceneSystemUnit = scene->GetGlobalSettings().GetSystemUnit();
    if( SceneSystemUnit.GetScaleFactor() != 1.0 )
    {
        FbxSystemUnit::cm.ConvertScene(scene);
    }
    

    LoadFBXObject(scene, OutFbxSkeletalMesh);
    OutFbxSkeletalMesh->name = FBXFilePath;

    // 애니메이션 정보 로드
    LoadFBXAnimations(scene, OutFbxSkeletalMesh, OutFbxSequences);

    LoadFBXCurves(scene, OutFbxSkeletalMesh, OutAnimStacks);
    scene->Destroy();
    //UE_LOG(ELogLevel::Display, "FBX parsed: %s", *FBXFilePath);
    return true;
}

// FFbxSkeletalMesh -> USkeletalMesh
// 등록은 외부에서 해야합니다.
void FFbxLoader::GenerateSkeletalMesh(const FFbxSkeletalMesh* InFbxSkeletal, USkeletalMesh*& OutSkeletalMesh)
{
    // .bin 또는 .fbx 파일에서 파싱한 FFbxSkeletalMesh를 USkeletalMesh로 변환
    OutSkeletalMesh = FObjectFactory::ConstructObject<USkeletalMesh>(nullptr);

    FSkeletalMeshRenderData renderData;
    renderData.ObjectName = InFbxSkeletal->name;
    renderData.RenderSections.SetNum(InFbxSkeletal->mesh.Num());
    renderData.MaterialSubsets.SetNum(InFbxSkeletal->materialSubsets.Num());

    for (int i = 0; i < InFbxSkeletal->mesh.Num(); ++i)
    {
        // TArray로 직접 접근해도 돼나?
        // 두 구조체의 메모리 레이아웃이 같아야함.
        renderData.RenderSections[i].Vertices.SetNum(InFbxSkeletal->mesh[i].vertices.Num());
        for (int j = 0; j < InFbxSkeletal->mesh[i].vertices.Num(); ++j)
        {
            renderData.RenderSections[i].Vertices[j].Position = InFbxSkeletal->mesh[i].vertices[j].position;
            renderData.RenderSections[i].Vertices[j].Color = InFbxSkeletal->mesh[i].vertices[j].color;
            renderData.RenderSections[i].Vertices[j].Normal = InFbxSkeletal->mesh[i].vertices[j].normal;
            renderData.RenderSections[i].Vertices[j].Tangent = InFbxSkeletal->mesh[i].vertices[j].tangent;
            renderData.RenderSections[i].Vertices[j].UV = InFbxSkeletal->mesh[i].vertices[j].uv;
            renderData.RenderSections[i].Vertices[j].MaterialIndex = InFbxSkeletal->mesh[i].vertices[j].materialIndex;
            memcpy(
                renderData.RenderSections[i].Vertices[j].BoneIndices,
                InFbxSkeletal->mesh[i].vertices[j].boneIndices,
                sizeof(int) * 8
            );
            memcpy(
                renderData.RenderSections[i].Vertices[j].BoneWeights,
                InFbxSkeletal->mesh[i].vertices[j].boneWeights,
                sizeof(float) * 8
            );
        }
        memcpy(
            renderData.RenderSections[i].Vertices.GetData(),
            InFbxSkeletal->mesh[i].vertices.GetData(),
            sizeof(FFbxVertex) * InFbxSkeletal->mesh[i].vertices.Num()
        );

        renderData.RenderSections[i].Indices = InFbxSkeletal->mesh[i].indices;
        renderData.RenderSections[i].SubsetIndex = InFbxSkeletal->mesh[i].subsetIndex;
        renderData.RenderSections[i].Name = InFbxSkeletal->mesh[i].name;
    }
    for (int i = 0; i < InFbxSkeletal->materialSubsets.Num(); ++i)
    {
        renderData.MaterialSubsets[i] = InFbxSkeletal->materialSubsets[i];
    }

    FReferenceSkeleton refSkeleton;
    refSkeleton.RawRefBoneInfo.SetNum(InFbxSkeletal->skeleton.joints.Num());
    refSkeleton.RawRefBonePose.SetNum(InFbxSkeletal->skeleton.joints.Num());

    for (int i = 0; i < InFbxSkeletal->skeleton.joints.Num(); ++i)
    {
        refSkeleton.RawRefBoneInfo[i].Name = InFbxSkeletal->skeleton.joints[i].name;
        refSkeleton.RawRefBoneInfo[i].ParentIndex = InFbxSkeletal->skeleton.joints[i].parentIndex;
        refSkeleton.RawRefBonePose[i].Translation = InFbxSkeletal->skeleton.joints[i].position;
        refSkeleton.RawRefBonePose[i].Rotation = InFbxSkeletal->skeleton.joints[i].rotation;
        refSkeleton.RawRefBonePose[i].Scale3D = InFbxSkeletal->skeleton.joints[i].scale;
        refSkeleton.RawNameToIndexMap.Add(InFbxSkeletal->skeleton.joints[i].name, i);
    }

    TArray<UMaterial*> Materials = InFbxSkeletal->material;

    TArray<FMatrix> InverseBindPoseMatrices;
    InverseBindPoseMatrices.SetNum(InFbxSkeletal->skeleton.joints.Num());
    for (int i = 0; i < InFbxSkeletal->skeleton.joints.Num(); ++i)
    {
        InverseBindPoseMatrices[i] = InFbxSkeletal->skeleton.joints[i].inverseBindPose;
    }
    OutSkeletalMesh->SetData(renderData, refSkeleton, InverseBindPoseMatrices, Materials);
    if (InverseBindPoseMatrices.Num() > 128)
    {
        // GPU Skinning: 최대 bone 개수 128개를 넘어가면 CPU로 전환
        OutSkeletalMesh->SetCPUSkinned(true);
    }

    //ParseFBXAnimationOnly(filename, newSkeletalMesh); // 이 호출에서 애니메이션 정보만 파싱됨
}

// FFbxAnimSequence와 FFbxAnimStack을 이름 기준으로 매칭합니다.
// 이름에 맞게 하나씩 UAnimDataModel을 생성합니다.
void FFbxLoader::GenerateAnimations(
    const FFbxSkeletalMesh* InFbxSkeletal,
    const FFbxAnimSequence* InFbxAnim,
    FString& OutAnimName,
    UAnimSequence*& OutAnimSequence
    //const TArray<FFbxAnimSequence*>& InFbxAnimSequences,
    //const TArray<FFbxAnimStack*>& InFbxAnimStacks,
    //TArray<FString>& OutAnimNames,
    //TArray<UAnimDataModel*>& OutAnimData
)
{
    UAnimDataModel* DataModel = FObjectFactory::ConstructObject<UAnimDataModel>(nullptr);
    DataModel->PlayLength = InFbxAnim->Duration;
    DataModel->FrameRate = FFrameRate(InFbxAnim->FrameRate, 1);
    DataModel->NumberOfFrames = InFbxAnim->NumFrames;
    DataModel->NumberOfKeys = InFbxAnim->NumKeys;

    for (const FFbxAnimTrack& Track : InFbxAnim->AnimTracks)
    {
        int BoneIndex = InFbxSkeletal->GetBoneIndex(Track.BoneName);
        if (BoneIndex < 0)
        {
            continue;
        }
        FRawAnimSequenceTrack NewTrackData;
        NewTrackData.PosKeys = Track.PosKeys;
        NewTrackData.RotKeys = Track.RotKeys;
        NewTrackData.ScaleKeys = Track.ScaleKeys;

        FBoneAnimationTrack NewTrack;
        NewTrack.Name = InFbxAnim->Name;
        NewTrack.BoneTreeIndex = BoneIndex;
        NewTrack.InternalTrackData = NewTrackData;

        DataModel->BoneAnimationTracks.Emplace(NewTrack);
    }

    UAnimSequence* AnimSequence = FObjectFactory::ConstructObject<UAnimSequence>(nullptr);
    AnimSequence->SetDataModel(DataModel);
    AnimSequence->SetName(InFbxAnim->Name);
    AnimSequence->SetSequenceLength(InFbxAnim->Duration);
    
    OutAnimSequence = AnimSequence;
    OutAnimName = InFbxAnim->Name;
}

// FbxScene -> FFbxSkeletalMesh
void FFbxLoader::LoadFBXObject(FbxScene* InFbxScene, FFbxSkeletalMesh* OutFbxSkeletalMesh)
{
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
        LoadFbxSkeleton(OutFbxSkeletalMesh, node, boneNameToIndex, -1);
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
        LoadFBXMesh(OutFbxSkeletalMesh, node, boneNameToIndex, weightMaps[i]);
        LoadFBXMaterials(OutFbxSkeletalMesh, node);
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

        // 이거 틀렸음..
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
    //FMatrix Mat;
    //for (int i = 0; i < 4; ++i)
    //    for (int j = 0; j < 4; ++j)
    //        Mat.M[i][j] = static_cast<float>(LocalTransform[i][j]);
    //FTransform Transform(Mat);
    FTransform Transform = FTransformFromFbxMatrix(LocalTransform);

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

// ControlIndex -> [BoneIndex, Weight]
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
//void FFbxLoader::ParseFBXAnimationOnly(const FString& filename, USkeletalMesh* skeletalMesh, TArray<UAnimSequence*>& OutSequences)
//{
//    FbxScene* scene = FbxScene::Create(Manager, "");
//    FbxImporter* importer = FbxImporter::Create(Manager, "");
//
//    if (!importer->Initialize(GetData(filename), -1, GetFbxIOSettings()))
//    {
//        importer->Destroy();
//        return;
//    }
//
//    if (!importer->Import(scene))
//    {
//        importer->Destroy();
//        return;
//    }
//
//    importer->Destroy();
//
//    FbxAxisSystem sceneAxisSystem = scene->GetGlobalSettings().GetAxisSystem();
//    // Unreal Engine: Z-Up, X-Forward, Left-Handed
//    if (sceneAxisSystem != UnrealTargetAxisSystem)
//    {
//        UE_LOG(ELogLevel::Display, "ParseFBXAnimationOnly: Converting axis system for animation.");
//        UnrealTargetAxisSystem.ConvertScene(scene);
//    }
//
//    FbxSystemUnit sceneSystemUnit = scene->GetGlobalSettings().GetSystemUnit();
//    // FBX SDK에서 cm의 스케일 팩터는 1.0입니다. 다른 단위인 경우 변환합니다.
//    if (sceneSystemUnit.GetScaleFactor() != 1.0)
//    {
//        UE_LOG(ELogLevel::Display, "ParseFBXAnimationOnly: Converting system unit to cm for animation.");
//        FbxSystemUnit::cm.ConvertScene(scene); // 씬 단위를 센티미터로 변환
//    }
//
//    LoadAnimationInfo(scene, OutSequences);
//    for (UAnimSequence* Sequence : OutSequences)
//    {
//        LoadAnimationData(scene, scene->GetRootNode(), skeletalMesh, Sequence);
//        //AnimMap.Add(Sequence->GetSeqName(), { LoadState::Completed, Sequence });
//    }
//
//#ifdef DEBUG_DUMP_ANIMATION
//    DumpAnimationDebug(filename, skeletalMesh, OutSequences);
//#endif
//
//    scene->Destroy();
//}

void FFbxLoader::LoadFBXAnimations(FbxScene* Scene, FFbxSkeletalMesh* fbxObject, TArray<FFbxAnimSequence*>& OutSequences)
{
    FbxArray<FbxString*> animNames;
    Scene->FillAnimStackNameArray(animNames);

    for (int i = 0; i < animNames.Size(); ++i)
    {
        // AnimStack(애니메이션 클립) 이름으로 AnimStack 가져오기
        FbxAnimStack* AnimStack = Scene->FindMember<FbxAnimStack>(animNames[i]->Buffer());
        Scene->SetCurrentAnimationStack(AnimStack); // 이게 빠져있음!
        if (!AnimStack) continue;

        FbxTakeInfo* TakeInfo = Scene->GetTakeInfo(AnimStack->GetName());
        if (!TakeInfo) continue;
        FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>();
        if (!AnimLayer) continue;

        FFbxAnimSequence* Sequence = new FFbxAnimSequence;
        Sequence->Name = FString(AnimStack->GetName());
        Sequence->Duration = static_cast<float>(TakeInfo->mLocalTimeSpan.GetDuration().GetSecondDouble());
        Sequence->FrameRate = static_cast<int32>(FbxTime::GetFrameRate(Scene->GetGlobalSettings().GetTimeMode()));
        Sequence->NumFrames = FMath::RoundToInt(Sequence->Duration * Sequence->FrameRate);
        Sequence->NumKeys = Sequence->NumFrames;
        
        TArray<FbxNode*> BoneNodes;
        CollectAllBoneNodes(Scene->GetRootNode(), fbxObject->skeleton, BoneNodes);
        for (FbxNode* BoneNode : BoneNodes)
        {
            FFbxAnimTrack Track;
            Track.BoneName = BoneNode->GetName();

            Track.PosKeys.SetNum(Sequence->NumFrames);
            Track.RotKeys.SetNum(Sequence->NumFrames);
            Track.ScaleKeys.SetNum(Sequence->NumFrames);

            for (int32 FrameIdx = 0; FrameIdx < Sequence->NumFrames; ++FrameIdx)
            {
                FbxTime Time;
                Time.SetSecondDouble(FrameIdx * (Sequence->Duration / Sequence->NumFrames));

                FbxAMatrix LocalMatrix = BoneNode->EvaluateLocalTransform(Time);
                FTransform Transform = FTransformFromFbxMatrix(LocalMatrix);

                FVector Pos = Transform.Translation;
                FQuat Rot = Transform.Rotation;
                FVector Scale = Transform.Scale3D;

                Track.PosKeys[FrameIdx] = Pos;
                Track.RotKeys[FrameIdx] = Rot;
                Track.ScaleKeys[FrameIdx] = Scale;
            }
            Sequence->AnimTracks.Emplace(Track);
        }
        OutSequences.Add(Sequence);
    }

    for (int i = 0; i < animNames.Size(); ++i)
        delete animNames[i];
    animNames.Clear();

    UE_LOG(ELogLevel::Display, "Loaded %d animation sequence(s).", OutSequences.Num());
    for (const FFbxAnimSequence* Seq : OutSequences)
    {
        UE_LOG(ELogLevel::Display, "  → Sequence: %s, Duration: %.2f, Frames: %d",
            *Seq->Name,
            Seq->Duration,
            Seq->NumFrames);
    }
}

void FFbxLoader::LoadFBXCurves(FbxScene* Scene, FFbxSkeletalMesh* fbxObject, TArray<FFbxAnimStack*>& OutAnimStacks)
{
    if (!Scene || !fbxObject)
        return;

    int AnimStackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
    for (int StackIdx = 0; StackIdx < AnimStackCount; ++StackIdx)
    {
        FbxAnimStack* AnimStack = Scene->GetSrcObject<FbxAnimStack>(StackIdx);
        if (!AnimStack) continue;

        FFbxAnimStack* NewAnimStack = new FFbxAnimStack;

        int AnimLayerCount = AnimStack->GetMemberCount<FbxAnimLayer>();
        for (int LayerIdx = 0; LayerIdx < AnimLayerCount; ++LayerIdx)
        {
            FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(LayerIdx);
            if (!AnimLayer) continue;

            FFbxAnimLayer NewAnimLayer;

            // Traverse all nodes (bones, meshes, etc)
            FbxNode* RootNode = Scene->GetRootNode();
            std::function<void(FbxNode*)> Traverse;
            Traverse = [&](FbxNode* Node)
                {
                    if (!Node) return;
                    FString BoneName = Node->GetName();

                    FbxAnimCurve* CurveTX = Node->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
                    FbxAnimCurve* CurveTY = Node->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
                    FbxAnimCurve* CurveTZ = Node->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);

                    FbxAnimCurve* CurveRX = Node->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
                    FbxAnimCurve* CurveRY = Node->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
                    FbxAnimCurve* CurveRZ = Node->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);

                    FbxAnimCurve* CurveSX = Node->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X);
                    FbxAnimCurve* CurveSY = Node->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y);
                    FbxAnimCurve* CurveSZ = Node->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z);

                    if ((CurveTX && CurveTX->KeyGetCount()) ||
                        (CurveTY && CurveTY->KeyGetCount()) ||
                        (CurveTZ && CurveTZ->KeyGetCount()) ||
                        (CurveRX && CurveRX->KeyGetCount()) ||
                        (CurveRY && CurveRY->KeyGetCount()) ||
                        (CurveRZ && CurveRZ->KeyGetCount()) ||
                        (CurveSX && CurveSX->KeyGetCount()) ||
                        (CurveSY && CurveSY->KeyGetCount()) ||
                        (CurveSZ && CurveSZ->KeyGetCount()))
                    {
                        FFbxAnimCurve TXCurve;
                        if (CurveTX) FFbxLoader::GetKeys(CurveTX, TXCurve.KeyFrames);
                        FFbxAnimCurve TYCurve;
                        if (CurveTY) FFbxLoader::GetKeys(CurveTY, TYCurve.KeyFrames);
                        FFbxAnimCurve TZCurve;
                        if (CurveTZ) FFbxLoader::GetKeys(CurveTZ, TZCurve.KeyFrames);

                        FFbxAnimCurve RXCurve;
                        if (CurveRX) FFbxLoader::GetKeys(CurveRX, RXCurve.KeyFrames);
                        FFbxAnimCurve RYCurve;
                        if (CurveRY) FFbxLoader::GetKeys(CurveRY, RYCurve.KeyFrames);
                        FFbxAnimCurve RZCurve;
                        if (CurveRZ) FFbxLoader::GetKeys(CurveRZ, RZCurve.KeyFrames);

                        FFbxAnimCurve SXCurve;
                        if (CurveSX) FFbxLoader::GetKeys(CurveSX, SXCurve.KeyFrames);
                        FFbxAnimCurve SYCurve;
                        if (CurveSY) FFbxLoader::GetKeys(CurveSY, SYCurve.KeyFrames);
                        FFbxAnimCurve SZCurve;
                        if (CurveSZ) FFbxLoader::GetKeys(CurveSZ, SZCurve.KeyFrames);

                        FFbxCurveNode CurveNode;
                        CurveNode.Curves.Add(FFbxCurveNode::ECurveChannel::TX, TXCurve);
                        CurveNode.Curves.Add(FFbxCurveNode::ECurveChannel::TY, TYCurve);
                        CurveNode.Curves.Add(FFbxCurveNode::ECurveChannel::TZ, TZCurve);
                        CurveNode.Curves.Add(FFbxCurveNode::ECurveChannel::RX, RXCurve);
                        CurveNode.Curves.Add(FFbxCurveNode::ECurveChannel::RY, RYCurve);
                        CurveNode.Curves.Add(FFbxCurveNode::ECurveChannel::RZ, RZCurve);
                        CurveNode.Curves.Add(FFbxCurveNode::ECurveChannel::SX, SXCurve);
                        CurveNode.Curves.Add(FFbxCurveNode::ECurveChannel::SY, SYCurve);
                        CurveNode.Curves.Add(FFbxCurveNode::ECurveChannel::SZ, SZCurve);
                        NewAnimLayer.AnimCurves.Add(BoneName, CurveNode);
                    }

                    for (int i = 0; i < Node->GetChildCount(); ++i)
                        Traverse(Node->GetChild(i));
                    return;
                };
            Traverse(RootNode);

            NewAnimStack->Name = FString(AnimStack->GetName());
            NewAnimStack->AnimLayers.Add(FString(AnimLayer->GetName()), NewAnimLayer);
            NewAnimStack->FrameRate = static_cast<float>(FbxTime::GetFrameRate(Scene->GetGlobalSettings().GetTimeMode()));
        }
        // 프레임레이트 추출 (Unreal Engine 소스 참고)

        OutAnimStacks.Add(NewAnimStack);
    }
}

void FFbxLoader::GetKeys(FbxAnimCurve* Curve, TArray<FFbxAnimCurveKey>& OutKeys)
{
    int KeyCount = Curve->KeyGetCount();
    OutKeys.SetNum(KeyCount);
    for (int k = 0; k < KeyCount; ++k)
    {
        FbxAnimCurveKey FbxKey = Curve->KeyGet(k);
        FFbxAnimCurveKey Key;
        Key.Time = static_cast<float>(FbxKey.GetTime().GetSecondDouble());
        Key.Value = FbxKey.GetValue();
        Key.Type = static_cast<FFbxAnimCurveKey::EInterpolationType>(FbxKey.GetInterpolation());
        Key.ArriveTangent = FbxKey.GetDataFloat(FbxAnimCurveDef::eRightSlope);
        Key.LeaveTangent = FbxKey.GetDataFloat(FbxAnimCurveDef::eNextLeftSlope);
        OutKeys[k] = Key;
    }
}

//void FFbxLoader::LoadAnimationInfo(FbxScene* Scene, TArray<UAnimSequence*>& OutSequences)
//{
//    FbxArray<FbxString*> animNames;
//    Scene->FillAnimStackNameArray(animNames);
//
//    for (int i = 0; i < animNames.Size(); ++i)
//    {
//        const char* currentFbxStackName = animNames[i]->Buffer();
//        FbxAnimStack* animStack = Scene->FindMember<FbxAnimStack>(currentFbxStackName); // 변수명 변경 (animStack -> fbxAnimStack)
//        if (!animStack) {
//            UE_LOG(ELogLevel::Warning, "Could not find AnimStack in scene: %s", currentFbxStackName);
//            continue;
//        }
//
//        FbxTakeInfo* takeInfo = Scene->GetTakeInfo(animStack->GetName());
//        if (!takeInfo) { continue; }
//
//        UAnimSequence* Sequence = FObjectFactory::ConstructObject<UAnimSequence>(nullptr);
//        Sequence->SetName(FString(animStack->GetName()));
//
//        UAnimDataModel* DataModel = FObjectFactory::ConstructObject<UAnimDataModel>(Sequence);
//        Sequence->SetDataModel(DataModel);
//
//        const float Duration = (float)takeInfo->mLocalTimeSpan.GetDuration().GetSecondDouble();
//        const FFrameRate FrameRate((int32)FbxTime::GetFrameRate(Scene->GetGlobalSettings().GetTimeMode()), 1);
//
//
//        DataModel->PlayLength = Duration;
//        DataModel->FrameRate = FrameRate;
//        DataModel->NumberOfFrames = FMath::RoundToInt(Duration * FrameRate.Numerator / (float)FrameRate.Denominator);
//        DataModel->NumberOfKeys = DataModel->NumberOfFrames;
//
//        Sequence->SetSequenceLength( Duration);
//
//        OutSequences.Add(Sequence);
//    }
//
//    for (int i = 0; i < animNames.Size(); ++i)
//        delete animNames[i];
//    animNames.Clear();
//
//    UE_LOG(ELogLevel::Display, "Loaded %d animation sequence(s).", OutSequences.Num());
//    for (UAnimSequence* Seq : OutSequences)
//    {
//        UE_LOG(ELogLevel::Display, "  → Sequence: %s, Duration: %.2f, Frames: %d",
//            *Seq->GetSeqName(),
//            Seq->GetDataModel()->PlayLength,
//            Seq->GetDataModel()->NumberOfFrames);
//    }
//}
//
//
//void FFbxLoader::LoadAnimationData(FbxScene* Scene, FbxNode* RootNode, USkeletalMesh* SkeletalMesh, UAnimSequence* OutSequ ence)
//{
//     if (!OutSequence || !OutSequence->GetDataModel() || !SkeletalMesh)
//         return;
//
//     auto NameDebug = *OutSequence->GetSeqName();
//     FbxAnimStack* AnimStack = Scene->FindMember<FbxAnimStack>(*OutSequence->GetSeqName());
//     if (!AnimStack)
//     {
//         UE_LOG(ELogLevel::Warning, "AnimStack not found for sequence: %s", *OutSequence->GetSeqName());
//         return;
//     }
//
//     Scene->SetCurrentAnimationStack(AnimStack);
//     FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>();
//     if (!AnimLayer) return;
//
//     const int32 FrameCount = OutSequence->GetDataModel()->NumberOfFrames;
//     const float DeltaTime = OutSequence->GetDataModel()->PlayLength / FrameCount;
//
//    TArray<FBoneAnimationTrack>& Tracks = OutSequence->GetDataModel()->BoneAnimationTracks;
//    FReferenceSkeleton RefSkeleton;
//    SkeletalMesh->GetRefSkeleton(RefSkeleton);
//
//    Tracks.SetNum(RefSkeleton.RawRefBoneInfo.Num());
//
//    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.RawRefBoneInfo.Num(); ++BoneIndex)
//    {
//        const FName& BoneName = RefSkeleton.RawRefBoneInfo[BoneIndex].Name;
//        FbxNode* BoneNode = FindBoneNode(RootNode, BoneName.ToString());
//        if (!BoneNode)
//        {
//            UE_LOG(ELogLevel::Warning, "BoneNode not found in FBX for bone: %s", *BoneName.ToString());
//            continue;
//        }
//
//        FBoneAnimationTrack Track;
//        Track.BoneTreeIndex = BoneIndex;
//        Track.Name = BoneName;
//
//        FRawAnimSequenceTrack& Raw = Track.InternalTrackData;
//        Raw.PosKeys.SetNum(FrameCount);
//        Raw.RotKeys.SetNum(FrameCount);
//        Raw.ScaleKeys.SetNum(FrameCount);
//
//        for (int32 FrameIdx = 0; FrameIdx < FrameCount; ++FrameIdx)
//        {
//            FbxTime Time;
//            Time.SetSecondDouble(FrameIdx * DeltaTime);
//
//            FbxAMatrix LocalMatrix = BoneNode->EvaluateLocalTransform(Time);
//            FTransform Transform = FTransformFromFbxMatrix(LocalMatrix);
//
//            Raw.PosKeys[FrameIdx] = Transform.Translation;
//            Raw.RotKeys[FrameIdx] = Transform.Rotation;
//            Raw.ScaleKeys[FrameIdx] = Transform.Scale3D;
//        }
//
//        Tracks[BoneIndex] = std::move(Track);
//    }
//}

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

//void FFbxLoader::LoadFBXAnimations(FFbxSkeletalMesh* fbxObject, FbxScene* scene, TArray<FFbxAnimTrack>& OutFbxAnimTracks)
//{
//    FbxAxisSystem sceneAxisSystem = scene->GetGlobalSettings().GetAxisSystem();
//    // Unreal Engine: Z-Up, X-Forward, Left-Handed
//    if (sceneAxisSystem != UnrealTargetAxisSystem)
//    {
//        UE_LOG(ELogLevel::Display, "ParseFBXAnimationOnly: Converting axis system for animation.");
//        UnrealTargetAxisSystem.ConvertScene(scene);
//    }
//
//    FbxSystemUnit sceneSystemUnit = scene->GetGlobalSettings().GetSystemUnit();
//    // FBX SDK에서 cm의 스케일 팩터는 1.0입니다. 다른 단위인 경우 변환합니다.
//    if (sceneSystemUnit.GetScaleFactor() != 1.0)
//    {
//        UE_LOG(ELogLevel::Display, "ParseFBXAnimationOnly: Converting system unit to cm for animation.");
//        FbxSystemUnit::cm.ConvertScene(scene); // 씬 단위를 센티미터로 변환
//    }
//
//    LoadAnimationInfo(scene, OutSequences);
//    for (UAnimSequence* Sequence : OutSequences)
//    {
//        LoadAnimationData(scene, scene->GetRootNode(), skeletalMesh, Sequence);
//        //AnimMap.Add(Sequence->GetSeqName(), { LoadState::Completed, Sequence });
//    }
//
//#ifdef DEBUG_DUMP_ANIMATION
//    DumpAnimationDebug(filename, skeletalMesh, OutSequences);
//#endif
//
//    scene->Destroy();
//}


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

    
