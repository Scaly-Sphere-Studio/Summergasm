log_msg("audio_setup.lua start");

Audio.setVolume(50)

bat_la_rate = Audio.Buffer.new("resources/sounds/bat-la-rate.mp3")
ok = Audio.Buffer.new("resources/sounds/ok.mp3")

source = Audio.Source.new()
source.loop = true;
source:queueBuffers({bat_la_rate.id, ok.id})
--source.volume = 30
--source:play()

log_msg("audio_setup.lua end");