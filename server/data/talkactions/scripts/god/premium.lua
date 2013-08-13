local helpCommandStart
local parseParameters
local pluralize
local respond
local respondPremiumStatus


function onSay(actorId, command, parameters, channel)
	parameters = parseParameters(parameters)
	
	local name = parameters[1]
	if name then
		local playerId = getPlayerByName(name)
		if not playerId then
			respond(actorId, "There is no player named '" .. name .. "' online.")
			return true
		end
		
		name = getCreatureName(playerId)
		
		local subcommand = parameters[2]
		if subcommand then
			local actorName = getCreatureName(actorId)
			
			local days = tonumber(parameters[3])
			if days then
				local daysText = pluralize(days, "one day", "days")
				
				if subcommand == "add" or subcommand == "remove" then
					if days <= 0 then
						respond(actorId, "Please give a positive number of premium days to " .. subcommand .. ".")
						return true
					end
					
					if playerHasUnlimitedPremium(playerId) then
						local start
						if playerId == actorId then
							start = "You have"
						else
							start = name .. " has"
						end
					
						respond(actorId, start .. " UNLIMITED premium time. Use  " .. helpCommandStart(command, name) .. " set <days>  instead.")
						return true
					end
					
					local what
					if playerId == actorId then
						what = "your premium time"
					else
						what = "the premium time of " .. name
					end
					
					if subcommand == "remove" then
						removePlayerPremiumDays(playerId, days)
						respond(actorId, "You removed " .. daysText .. " from " .. what .. ".")
						
						if playerId ~= actorId then
							respond(playerId, actorName .. " removed " .. daysText .. " from your premium time.")
						end
					else
						addPlayerPremiumDays(playerId, days)
						setPlayerStorageValue(playerId, 20500, 1)
						
						respond(actorId, "You added " .. daysText .. " to " .. what .. ".")
						
						if playerId ~= actorId then
							respond(playerId, actorName .. " added " .. daysText .. " to your premium time.")
						end
					end
					
					respondPremiumStatus(actorId, playerId, name, true)
					if playerId ~= actorId then
						respondPremiumStatus(playerId, playerId, name, true)
					end
					
					return true
				elseif subcommand == "set" then
					if days < 0 then
						respond(actorId, "Please give zero or more premium days.")
						return true
					end
					
					setPlayerPremiumDays(playerId, days)
					
					if days > 0 then
						setPlayerStorageValue(playerId, 20500, 1)
					end
					
					if playerId ~= actorId then
						if days == 0 then
							respond(playerId, actorName .. " removed all your premium time.")
						else
							respond(playerId, actorName .. " set your premium time to " .. daysText .. " which is until " .. getPlayerPremiumExpirationText(playerId) .. ".")
						end
					end
					
					respondPremiumStatus(actorId, playerId, name, true)
					
					return true
				end
			elseif subcommand == "set" and parameters[3] == "unlimited" then
				setPlayerUnlimitedPremium(playerId)
				setPlayerStorageValue(playerId, 20500, 1)
					
				if playerId ~= actorId then
					respond(playerId, actorName .. " just gave you UNLIMITED premium time!")
				end
				
				respondPremiumStatus(actorId, playerId, name, true)
				return true
			end
		else
			respondPremiumStatus(actorId, playerId, name)
			return true
		end
	end
	
	command = helpCommandStart(command, name)
	
	respond(actorId, "Usage:")
	respond(actorId, "     " .. command .. "    -- see whether the player has premium and for how long")
	respond(actorId, "     " .. command .. " add <days>    -- give the player additional premium time")
	respond(actorId, "     " .. command .. " set <days>    -- set the player's premium time to exactly the given number of days")
	respond(actorId, "     " .. command .. " set unlimited    -- give the player unlimited premium time")
	respond(actorId, "     " .. command .. " remove <days>    -- reduce the player's premium time by the given time")
	
	return true
end


function helpCommandStart(command, name)
	if not name then
		name = "<player>"
	elseif name:find("%s") then
		if name:find('"') then
			name = "'" .. name .. "'"
		else
			name = '"' .. name .. '"'
		end
	end
	
	return command .. " " .. name
end


function parseParameters(parameters)
	local parsedParameters = {}
	
	while true do
		local start, stop, value = parameters:find('^%s*"%s*(.-)%s*"')
		if not start then
			start, stop, value = parameters:find("^%s*'%s*(.-)%s*'")
			if not start then
				start, stop, value = parameters:find("^%s*([^\"'%s]+)")
				if not start then break end
			end
		end
		
		parameters = parameters:sub(stop + 1)
		parsedParameters[#parsedParameters + 1] = value
	end
	
	return parsedParameters
end


function pluralize(value, singular, plural)
	if not value then
		return value
	end
	
	if value == 1 then
		return singular
	else
		return value .. " " .. plural
	end
end


function respond(actorId, text)
	doPlayerSendTextMessage(actorId, MESSAGE_STATUS_CONSOLE_ORANGE, text)
end


function respondPremiumStatus(actorId, playerId, name, changed)
	local have
	if playerId == actorId then
		have = "have"
		name = "You"
	else
		have = "has"
	end
	
	local start = name .. " " .. have
	
	local now
	if changed then
		now = " now"
	else
		now = ""
	end
	
	local response
	if playerHasPremium(playerId) then
		if playerHasUnlimitedPremium(playerId) then
			response = start .. " UNLIMITED premium time" .. now .. "."
		else
			response = start .. " about " .. pluralize(getPlayerPremiumDays(playerId), "one day", "days") .. " premium time until "
			           .. getPlayerPremiumExpirationText(playerId) .. now .. "."
		end
	elseif changed then
		response = start .. " no more premium time."
	else
		response = start .. " no premium time."
	end
	
	respond(actorId, response)
end
