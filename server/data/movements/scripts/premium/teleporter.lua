function willAddCreature(position, creatureId, actorId)
	if not playerHasPremium(creatureId) then
		doPlayerSendTextMessage(creatureId, MESSAGE_STATUS_CONSOLE_ORANGE, "You must have premium time to enter the premium area.")
		return RETURNVALUE_DESTINATIONOUTOFREACH
	end
end
