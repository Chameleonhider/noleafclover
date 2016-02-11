2016.01.XX OpenSpades release

To access settings in-game, press "Global say <T>" or "</>" keys. Then write down the name of the setting you want to change, a list should pop up, unless you make a typo.

Settings:
|Name:					|Possible values/range: |Explanation:
|	----		----	|	----		----	|	----	----	----
|cg_mouseCrouchScale	|0-1					|Mouse sensitivity scale when crouching. default 0.75
|cg_keyWeaponMode		|<key> default <x>		|Switches SMG firemodes. Can be Auto>Semi or Auto>Burst>Semi
|weap_burstFire			|0						|Enable(1)/Disable(0) SMG burst fire mode
|weap_burstRounds		|-1-30					|How many rounds does burst firemode produce(default 3)
|						|						|
|	----		----	|	----		----	|	----	----	----
View/HUD settings:
|Name:					|Possible values/range: |Explanation:
|	----		----	|	----		----	|	----	----	----
|v_playerDlines			|0,1,2					|Display teammate highlight. 0-disable, 1-enable precise, 2-enable all
|v_playerNames			|0,1,2					|Display teammate names. 0-disable, 1-show distance, 2-show only name
|v_playerNameX			|range +-400			|Displayed teammate name X location on screen. 0 is center.
|v_playerNameY			|range +-300			|Displayed teammate name Y location on screen. 0 is center.
|v_hitVibration			|0,1					|Close hit view vibration. 0-disable, 1-enable
|v_defaultFireVibration	|0,1					|Toggles between new and old weapon shake. 0-disable, 1-enable
|v_defaultSprintBob		|0,1					|Toggles between new and old sprint bobbing. 0-disable, 1-enable
|v_blockDlines			|0,1,2					|Shows white/yellow/red lines when building. 0-disable, 1-enable white&red, 2-enable all
|						|						|
|hud_centerMessage		|0,1,2					|Controls the big center messages. 0-disable, 1-enable ctf/tc, 2-enable all (ctf/tc + kills)
|						|						|
|	----		----	|	----		----	|	----	----	----
Optimization settings:
|Name:					|Possible values/range: |Explanation:
|	----		----	|	----		----	|	----	----	----
|opt_particleMaxDist	|0-150 (usually 125)	|Maximum particle display distance in blocks. For all particles.
|opt_particleNiceDist	|0-???			(def 1)	|Scales number of nice looking particles produced. 0-no particles will be produced, 0.5-half, 1-default.
|opt_particleNumScale	|0-guise, look!	(def 1) |Scales number of all particles produced. 0-no particles will be produced, 0.5-half, 1-default.
|opt_muzzleFlash		|0,1					|Toggles default muzzleflash on/off. There is no custom flash right now. 0-disable, 1-enable.
|opt_particleFallBlockReduce|0,1,2				|Reduce the amount of particles falling blocks produce. 0-no reduction, 1-constant reduction, 2-reduction above 250 blocks
|opt_brassTime			|0-guise, look!			|Sets how long spent shells lay on the ground, in seconds. Try doing 6000 on full server.
|opt_tracers			|0,1,2					|Togles display of tracers. 0-disable, 1-enable, 2-enable .kv6 tracers. FUCK YEA!
|						|						|
|snd_maxDistance		|0-1024					|Controls how far the sounds can be heard from in blocks. Also mind that when shooting, player will lose hearing, which he will gain back.
|						|						|
|	----		----	|	----		----	|	----	----	----