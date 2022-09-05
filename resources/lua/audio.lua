log_msg("audio.lua start");

--setMainVolume(50)

Audio_Buffer.create(0);
Audio_Buffer.create(1);
Audio_Buffer.get(0):loadFile("resources/sounds/bat-la-rate.mp3")
Audio_Buffer.get(1):loadFile("resources/sounds/ok.mp3")

Audio_Source.create(0)
source = Audio_Source.get(0)

source.loop = true;
source:queueBuffers({0, 1})
--source.volume = 30
--source:play()

log_msg("audio.lua end");