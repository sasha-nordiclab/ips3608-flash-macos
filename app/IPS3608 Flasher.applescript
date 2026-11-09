-- IPS3608 Flasher: standalone GUI. Bundles the ips3608flash binary in Resources.
-- Built into an .app by app/build.sh.

on run
	set ru to (user locale of (get system info)) starts with "ru"
	set appTitle to "IPS3608 Flasher"
	set tool to POSIX path of (path to resource "ips3608flash")

	-- 1. Wait for the bootloader disk
	if ru then
		set waitMsg to "Подключи блок в режиме прошивки:" & return & return & "1. Выключи блок." & return & "2. Зажми ВЕРХНЮЮ кнопку и, не отпуская, включи питание." & return & "3. На экране появится Firmware Upgrade." & return & "4. Подключи ЗАДНИЙ USB-порт к Mac кабелем ИЗ КОРОБКИ." & return & return & "Жду блок…"
		set btns to {"Отмена", "Проверить"}
	else
		set waitMsg to "Connect the IPS3608 in upgrade mode:" & return & return & "1. Turn the unit off." & return & "2. Hold the UPPER button and turn the power on." & return & "3. The Firmware Upgrade screen appears." & return & "4. Connect the REAR USB port with the cable FROM THE BOX." & return & return & "Waiting for the device…"
		set btns to {"Cancel", "Check"}
	end if
	set findCmd to "for d in $(diskutil list external physical | awk '/^\\/dev\\/disk/{print $1}'); do diskutil info $d | grep -q 'Media Name: *Msc iap' && echo $d; done; true"
	repeat
		set found to paragraphs of (do shell script findCmd)
		if (count of found) is 1 then exit repeat
		if (count of found) > 1 then
			if ru then
				display dialog "Подключено несколько блоков в режиме прошивки. Оставь один." buttons {"OK"} default button 1 with title appTitle with icon stop
			else
				display dialog "More than one IPS3608 in upgrade mode is connected. Leave only one." buttons {"OK"} default button 1 with title appTitle with icon stop
			end if
			return
		end if
		display dialog waitMsg buttons btns default button 2 cancel button 1 with title appTitle with icon note giving up after 3
	end repeat
	set disk to item 1 of found

	-- 2. Pick firmware
	if ru then
		set pickMsg to "Блок найден. Выбери файл прошивки APP_IPS3608_….bin"
	else
		set pickMsg to "Device found. Choose the firmware file APP_IPS3608_….bin"
	end if
	set fw to POSIX path of (choose file with prompt pickMsg default location (path to downloads folder))
	set fwName to do shell script "basename " & quoted form of fw
	if fwName does not start with "APP_IPS3608_" or fwName does not end with ".bin" then
		if ru then
			set msg to "Это не прошивка IPS3608: " & fwName & return & "Нужен файл вида APP_IPS3608_V1.9_260429.bin"
		else
			set msg to "Not an IPS3608 firmware file: " & fwName & return & "Expected something like APP_IPS3608_V1.9_260429.bin"
		end if
		display dialog msg buttons {"OK"} default button 1 with title appTitle with icon stop
		return
	end if

	-- 3. Confirm
	if ru then
		display dialog "Прошить блок файлом" & return & fwName & "?" buttons {"Нет", "Да"} default button 2 cancel button 1 with title appTitle
	else
		display dialog "Flash the IPS3608 with" & return & fwName & "?" buttons {"No", "Yes"} default button 2 cancel button 1 with title appTitle
	end if

	-- 4. Flash (macOS asks for the admin password)
	set cmd to "diskutil unmountDisk force " & disk & " >/dev/null && " & quoted form of tool & " " & quoted form of ("/dev/r" & text 6 thru -1 of disk) & " " & quoted form of fw & " 2>&1"
	try
		set out to do shell script cmd with administrator privileges
	on error errMsg number errNum
		if errNum is -128 then return
		if errMsg contains "nothing to write" then
			if ru then
				set msg to "Эта прошивка уже лежит на блоке." & return & return & "Выключи блок, снова войди в режим прошивки и запусти программу ещё раз."
			else
				set msg to "This firmware is already on the device." & return & return & "Turn the unit off, re-enter upgrade mode and run the app again."
			end if
		else if ru then
			set msg to "Ошибка прошивки:" & return & return & errMsg & return & return & "Не нажимай RUN/STOP. Войди в режим прошивки заново и попробуй ещё раз."
		else
			set msg to "Flashing failed:" & return & return & errMsg & return & return & "Do NOT press RUN/STOP. Re-enter upgrade mode and try again."
		end if
		display dialog msg buttons {"OK"} default button 1 with title appTitle with icon stop
		return
	end try

	-- 5. Result
	if out contains "RESULT: VERIFIED" then
		if ru then
			set msg to "Прошивка записана и проверена." & return & return & "Теперь КОРОТКО нажми RUN/STOP на блоке." & return & "Не отключай USB и питание до перезагрузки."
		else
			set msg to "Firmware written and verified." & return & return & "Now SHORT-PRESS RUN/STOP on the IPS3608." & return & "Do not disconnect USB or power until it reboots."
		end if
	else if out contains "RESULT: AUTOFLASH" then
		if ru then
			set msg to "Прошивка записана. Блок сам начал обновление." & return & return & "Дождись перезагрузки и проверь версию в настройках." & return & "Не отключай USB и питание."
		else
			set msg to "Firmware written. The device started the update by itself." & return & return & "Wait for it to reboot and check the version in settings." & return & "Do not disconnect USB or power."
		end if
	else
		set msg to out
	end if
	display dialog msg buttons {"OK"} default button 1 with title appTitle with icon note
end run
