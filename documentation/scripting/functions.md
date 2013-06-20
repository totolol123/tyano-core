Scripting: Functions
====================


* `addPlayerPremiumDays (Number playerId, Number days)`    
	Adds premium days to the player's account.  
	The player's account must not have unlimited premium. You can test that with `playerHasUnlimitedPremium`.  
	Returns the new number of premium days on success, `false, String errorMessage` on failure.

* `getPlayerPremiumDays (Number playerId)`  
	The player's account must not have unlimited premium. You can test that with `playerHasUnlimitedPremium`.  
	Returns the number of premium days for a player on success, `false, String errorMessage` on failure.

* `getPlayerPremiumExpirationText (Number playerId)`  
	Returns a formatted text for the premium expiration of the player's account.
	* `unlimited` if the player's account has unlimited premium.
	* `none` if the player's account does not have premium.
	* Otherwise returns a formatted date for when the premium access expires.
	
* `playerHasPremium (Number playerId)`  
	Returns `true` if the player's account has premium, `false` otherwise.
	
* `playerHasUnlimitedPremium (Number playerId)`  
	Returns `true` if the player's account has unlimited premium, `false` otherwise.
	
* `removePlayerPremiumDays(Number playerId)`  
	Removes premium days from the player's account.  
	The player's account must not have unlimited premium. You can test that with `playerHasUnlimitedPremium`.  
	Returns the new number of premium days on success, `false, String errorMessage` on failure.
	
* `setPlayerPremiumDays (Number playerId, Number days)`  
	Sets the number of premium days for a player's account, overwriting all existing days and even unlimited premium.  
	Returns the new number of premium days on success.

* `setPlayerUnlimitedPremiumDays(Number playerId)`  
	Grants the player's account unlimited premium.
	Returns `true`.
