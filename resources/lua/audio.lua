log_msg("audio.lua start");

do
  local _ENV = Audio

  setVolume(50)
  
  BAT_LA_RATE = 0
  OK = 1

  Buffer.get(BAT_LA_RATE):loadFile("resources/sounds/bat-la-rate.mp3")
  Buffer.get(OK):loadFile("resources/sounds/ok.mp3")
  
  local source = Source.get(0)
  source.loop = true;
  source:queueBuffers({BAT_LA_RATE, OK})
  --source.volume = 30
  --source:play()
end

log_msg("audio.lua end");