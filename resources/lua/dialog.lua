-- Ren'Py dialog, played by the C++ Dialog (see Dialog.hpp for the controls).
-- Returns to the menu once the story is over, or on Escape.
if (is_loading)
then
    print(filename, "init start")

    dialog = Dialog.new("char/scene_vacances.rpy")

    -- Escape closes the dialog's log if it's open, instead of leaving
    function on_escape ()
        return dialog ~= nil and dialog.log_open
    end

    print(filename, "init end")

elseif (is_unloading)
then
    -- Destroyed (and its renderer removed) when collected
    dialog = nil

elseif (is_running)
then
    if (dialog ~= nil and dialog:update())
    then
        menu()
    end
end
