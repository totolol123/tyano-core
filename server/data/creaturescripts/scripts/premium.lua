local pluralize
local respond


function onLogin(playerId)
	if playerHasPremium(playerId) then
		setPlayerStorageValue(playerId, 20500, 1)
		
		if playerHasUnlimitedPremium(playerId) then
			respond(playerId, "Cool, you have UNLIMITED premium time!")
		else
			respond(playerId, "You have about "
			        .. pluralize(getPlayerPremiumDays(playerId), "one day", "days") .. " premium time until "
			        .. getPlayerPremiumExpirationText(playerId) .. ".")
		end
	elseif getPlayerStorageValue(playerId, 20500) == 1 then
		setPlayerStorageValue(playerId, 20500, nil)
		doTeleportThing(playerId, getPlayerMasterPos(playerId))		
		
		respond(playerId, "Too bad. Your premium time is over!")
		respond(playerId, "Get more on our website :)")
	else
		respond(playerId, "Too bad. You don't have any premium time!")
		respond(playerId, "Get more on our website :)")
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
