local pluralize
local respond


function onSay(actorId, command, parameters, channel)
	if playerHasPremium(actorId) then
		if playerHasUnlimitedPremium(actorId) then
			respond(actorId, "Cool, you have UNLIMITED premium time!")
		else
			respond(actorId, "You have about "
			        .. pluralize(getPlayerPremiumDays(actorId), "one day", "days") .. " premium time until "
			        .. getPlayerPremiumExpirationText(actorId) .. ".")
		end
	else
		respond(actorId, "Too bad, but you don't have any premium time.")
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
