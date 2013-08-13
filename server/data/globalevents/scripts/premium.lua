local pluralize
local respond

function onThink()
	for _, playerId in ipairs(getPlayersOnline()) do
		if not playerHasPremium(playerId) and getPlayerStorageValue(playerId, 20500) == 1 then
			setPlayerStorageValue(playerId, 20500, nil)
			doTeleportThing(playerId, getPlayerMasterPos(playerId))		
			
			respond(playerId, "Too bad. Your premium time is over!")
			respond(playerId, "Get more on our website :)")
		end
	end
	
	return true
end


function pluralize(value, singular, plural)
	if value == 1 then
		return singular
	else
		return value .. " " .. plural
	end
end


function respond(actorId, text)
	doPlayerSendTextMessage(actorId, MESSAGE_STATUS_CONSOLE_ORANGE, text)
end
