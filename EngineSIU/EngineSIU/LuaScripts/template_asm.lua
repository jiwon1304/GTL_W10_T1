States = {
    { Name = "Entry", Sequence = "" },
}

Transitions = {
    {
        From = "Entry",
        To = "Entry", -- Input first state name
        Condition = function()
            return true
        end
    }
}