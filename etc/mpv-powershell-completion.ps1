#!/bin/pwsh

# PowerShell command line completion for the mpv/mpv.net media player.

# It can be installed by dot sourcing it in the PowerShell profile.

#
# This file is part of mpv.
#
# mpv is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# mpv is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with mpv.  If not, see <http://www.gnu.org/licenses/>.
#

$Options = New-Object Collections.Generic.List[Object]
$UpdateOptions = @('vo', 'ao', 'profile', 'hwdec', 'audio-device', 'scale', 'tscale',
                   'error-diffusion', 'vulkan-device')

Function SetOptions
{
    try
    {
        $optionContent = mpv --no-config --list-options
    }
    catch
    {
        throw
    }

    foreach ($line in $optionContent)
    {
        $line = $line.Trim()

        if (-not $line.StartsWith("--"))
        {
            continue
        }

        $table = @{ value = ""; type = ""; choices = $null }

        if ($line.Contains(" "))
        {
            $table["name"] = $line.Substring(2, $line.IndexOf(" ") - 2)
            $value = $line.Substring($line.IndexOf(" ") + 1).Trim()

            if ($value.Contains("("))
            {
                $value = $value.Substring(0, $value.IndexOf("(")).TrimEnd()
            }

            $table["value"] = $value
        }
        else
        {
            $table["name"] = $line.Substring(2)
        }

        if ($value.StartsWith("Choices:"))
        {
            $table["type"] = "choice"
            $table["choices"] = $value.Substring(8).TrimStart() -split " "
        }

        if ($value.StartsWith('Flag'))
        {
            $table['type'] = 'flag'
        }

        if ($value.Contains('[file]') -or $table["name"].Contains('-file'))
        {
            $table['type'] = 'file'
        }

        if ($table['type'] -eq 'flag')
        {
            $noTable = @{ name = 'no-' + $table['name'];
                          value = $table['value'];
                          type = '';
                          choices = $null
                        }
            $Options.Add($table)
            $Options.Add($noTable)
        }
        else
        {
            $Options.Add($table)
        }
    }
}

Function Update-Option($name)
{
    foreach ($it in $Options)
    {
        if ($name -eq $it.name)
        {
            $option = $it
            break
        }
    }

    if ($null -eq $option)
    {
        Write-Error "Option $name is unknown."
        return
    }

    if ($null -ne $option.choices)
    {
        return
    }

    $dynamicOptions = @(
        @{ name = 'vulkan-device'; pattern = "^\s*('.+?')" },
        @{ name = 'audio-device'; pattern = "^\s*('\S+')" },
        @{ name = 'hwdec'; pattern = '^\s*([-\w]+)' },
        @{ name = 'error-diffusion'; pattern = '^\s*([-\w]+)' },
        @{ name = 'tscale'; pattern = '^\s*([-\w]+)' },
        @{ name = 'scale'; pattern = '^\s*([-\w]+)' },
        @{ name = 'profile'; pattern = '^\s*([-\w]+)' },
        @{ name = 'ao'; pattern = '^\s*([-\w]+)' },
        @{ name = 'vo'; pattern = '^\s*([-\w]+)' }
     )

    foreach ($opt in $dynamicOptions)
    {
        if ($name -eq $opt.name)
        {
            $output = mpv ('--' + $opt.name + '=help') | Select-Object -Skip 1 |
                 Select-String ($opt.pattern) -AllMatches |
                 ForEach-Object { $_.matches.Groups[1].Value } |
                 Select-Object -Unique | Sort-Object

            $output = $output | foreach { if ($_ -match "'\w+'") { $_ -replace "'", '' } else { $_ } }

            if ($output -is [string])
            {
                $output = @($output)
            }

            $output += @('help')
            $option.choices = $output
            $option.type = 'choice'
            break
        }
    }
}

Function Get-Completion($cursorPosition, $wordToComplete, $commandName)
{
    if ($Options.Count -eq 0)
    {
        SetOptions
    }

    if ($commandName.StartsWith('--'))
    {
        if ($commandName -like '--*-file*=')
        {
            return (Get-ChildItem -file).FullName | Resolve-Path -Relative |
                ForEach-Object { if ($_.Contains(' ')) { $commandName + "'$_'" } else { $commandName + $_ } }
        }

        if ($commandName -match '(--.+-file.*=)(.+)')
        {
            return (Get-ChildItem -file).FullName | Resolve-Path -Relative |
                Where-Object { $_.ToLower().Contains($Matches[2].ToLower()) } |
                ForEach-Object { if ($_.Contains(' ')) { $Matches[1] + "'$_'" } else { $Matches[1] + $_ } }
        }

        $shortCommandName = $commandName.Substring(2)

        $argName = ''

        if ($commandName.EndsWith('='))
        {
            $shortCommandName = $shortCommandName.Substring(0, $shortCommandName.Length -1)
        }
        elseif ($commandName.Contains('='))
        {
            $shortCommandName = $shortCommandName.Substring(0, $shortCommandName.IndexOf('='))
            $argName = $commandName.Substring($commandName.IndexOf('=') + 1)
        }

        foreach ($it in $UpdateOptions)
        {
            if ($shortCommandName -eq $it)
            {
                Update-Option $it
                break
            }
        }

        $results = New-Object Collections.Generic.List[Object]

        $exactMatches = $Options | Where-Object { $_.name -eq $shortCommandName }

        foreach ($it in $exactMatches)
        {
            if (-not $commandName.Contains('='))
            {
                continue
            }

            $arguments = $null

            if ($it.type -eq 'flag')
            {
                $arguments = 'yes', 'no'
            }

            if ($it.type -eq 'choice' -and $null -ne $it.choices)
            {
                $arguments = $it.choices
            }

            if ($null -ne $arguments)
            {
                foreach ($arg in $arguments)
                {
                    if ($argName -ne '')
                    {
                        if ($arg.StartsWith($argName))
                        {
                            $results.Add('--' + $it.name + '=' + $arg)
                        }
                    }
                    else
                    {
                        $results.Add('--' + $it.name + '=' + $arg)
                    }
                }
            }
        }

        if (-not $commandName.Contains('='))
        {
            $partlyMatches = $Options | Where-Object { $_.name.StartsWith($shortCommandName) }

            foreach ($it in $partlyMatches)
            {
                if ($it.name -eq $shortCommandName)
                {
                    continue
                }

                $results.Add('--' + $it.name)
            }
        }

        return $results
    }
    elseif ($commandName -eq '')
    {
        return (Get-ChildItem).FullName | Resolve-Path -Relative
    }
    else
    {
        return (Get-ChildItem).FullName | Resolve-Path -Relative |
            Where-Object { $_.ToLower().Contains($commandName.ToLower()) }
    }
}

Register-ArgumentCompleter -Native -CommandName mpv -ScriptBlock {
    param($commandName, $wordToComplete, $cursorPosition)

    Get-Completion $cursorPosition "$wordToComplete" "$commandName" | ForEach-Object {
        [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_)
    }
}

Register-ArgumentCompleter -Native -CommandName mpvnet -ScriptBlock {
    param($commandName, $wordToComplete, $cursorPosition)

    Get-Completion $cursorPosition "$wordToComplete" "$commandName" | ForEach-Object {
        [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_)
    }
}
