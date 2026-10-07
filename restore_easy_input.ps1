Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "  Restoring original easy_input keyboard firmware" -ForegroundColor Yellow
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Please make sure:"
Write-Host "1. Board is connected and turned ON"
Write-Host "2. Press and release BOOT button once"
Write-Host ""
Read-Host "Press Enter to start flashing..."

. C:\Espressif\tools\Microsoft.v5.5.5.PowerShell_profile.ps1
Set-Location "C:\Users\27200\Desktop\esay_input_pianokeys\easy_input\easy-input-maker"
esptool.py --chip esp32s3 -b 460800 write_flash --flash_mode dio --flash_size 16MB 0x0 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0x10000 build/easy_input_keyboard.bin

Write-Host ""
Write-Host "==================================================" -ForegroundColor Green
Write-Host "Flashing complete! Turn power switch OFF and ON to reboot." -ForegroundColor Green
Write-Host "==================================================" -ForegroundColor Green
pause
