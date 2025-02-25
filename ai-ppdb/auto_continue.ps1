# Add Windows Forms support
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName UIAutomation

Write-Host "Auto-continue script started..."
Write-Host "Press Ctrl+C to terminate"

# Display all process names for debugging
Write-Host "`nCurrent running processes:"
Get-Process | Select-Object ProcessName | Sort-Object ProcessName | Format-Table

$count = 0
while ($true) {
    # Try multiple possible process names
    $processes = Get-Process | Where-Object { 
        $_.ProcessName -match "cursor|Cursor|CURSOR|windsurf|Windsurf|WINDSURF" 
    }
    
    if ($processes) {
        foreach ($proc in $processes) {
            Write-Host "Found process: $($proc.ProcessName) (ID: $($proc.Id))"
            
            # Try to locate window
            $automation = [System.Windows.Automation.AutomationElement]::FromHandle($proc.MainWindowHandle)
            if ($automation) {
                $windowTitle = $automation.Current.Name
                Write-Host "Window title: $windowTitle"
                
                # Search for element with specific text
                $condition = New-Object System.Windows.Automation.PropertyCondition(
                    [System.Windows.Automation.AutomationElement]::NameProperty, 
                    "*tool calls*"
                )
                $element = $automation.FindFirst(
                    [System.Windows.Automation.TreeScope]::Descendants, 
                    $condition
                )
                
                if ($element) {
                    Write-Host "Found prompt text, sending response..."
                    [System.Windows.Forms.SendKeys]::SendWait("continue{ENTER}")
                    $count++
                    Write-Host "Responded $count times"
                }
            }
        }
    } else {
        if ($count % 10 -eq 0) {  # Show message every 10 loops
            Write-Host "No Cursor/Windsurf process found..."
        }
    }
    Start-Sleep -Milliseconds 1000
}
