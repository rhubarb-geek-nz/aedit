#!/usr/bin/env pwsh
#
#  Copyright 2023, Roger Brown
#
#  This file is part of RHB aedit.
#
#  This program is free software: you can redistribute it and/or modify it
#  under the terms of the GNU General Public License as published by the
#  Free Software Foundation, either version 3 of the License, or (at your
#  option) any later version.
# 
#  This program is distributed in the hope that it will be useful, but WITHOUT
#  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
#  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
#  more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>
#
#  $Id: package.ps1 75 2024-01-20 09:48:46Z rhubarb-geek-nz $

param(
	$CertificateThumbprint = '601A8B683F791E51F647D34AD102C38DA4DDB65F',
	$BundleThumbprint = '5F88DFB53180070771D4507244B2C9C622D741F8'
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

trap
{
	throw $PSItem
}

[int]$VersionNumber = ( Get-Content -LiteralPath 'src/aedit.c' | ForEach-Object { if ( $_.Contains('Id: aedit.c')) { $_ } } ).Trim().Split(' ')[3]
$VersionNumberHex = $VersionNumber.ToString('X8')
$list = @(
	( [int32]::Parse($VersionNumberHex.Substring(0,4),[System.Globalization.NumberStyles]::HexNumber) + 1),
	( [int32]::Parse($VersionNumberHex.Substring(4,2),[System.Globalization.NumberStyles]::HexNumber) + 1),
	( [int32]::Parse($VersionNumberHex.Substring(6,2),[System.Globalization.NumberStyles]::HexNumber) + 0)
)

$Version = Join-String -Separator '.' -InputObject $list

Write-Output "Version is $Version from $VersionNumber"

if ($IsLinux)
{
	& make clean

	If ( $LastExitCode -ne 0 )
	{
		Exit $LastExitCode
	}

	& make CFLAGS="-Wall -Werror" aedit

	If ( $LastExitCode -ne 0 )
	{
		Exit $LastExitCode
	}

	& strip aedit

	If ( $LastExitCode -ne 0 )
	{
		Exit $LastExitCode
	}

	& make CFLAGS="-Wall -Werror"

	If ( $LastExitCode -ne 0 )
	{
		Exit $LastExitCode
	}
}

if ($IsMacOS)
{
	& make clean

	If ( $LastExitCode -ne 0 )
	{
		Exit $LastExitCode
	}

	foreach ($Arch in 'arm64', 'x86_64')
	{
		if (Test-Path -LiteralPath 'config.h' -PathType Leaf)
		{
			Remove-Item -LiteralPath 'config.h'
		}

		& make CFLAGS="-Wall -Werror -pedantic -arch $Arch" aedit

		If ( $LastExitCode -ne 0 )
		{
			Exit $LastExitCode
		}

		& strip aedit
		
		If ( $LastExitCode -ne 0 )
		{
			Exit $LastExitCode
		}

		& codesign --timestamp --sign "Developer ID Application: $Env:APPLE_DEVELOPER" aedit

		If ( $LastExitCode -ne 0 )
		{
			Exit $LastExitCode
		}

		Move-Item -LiteralPath 'aedit' -Destination "aedit.$Arch"
	}

	& lipo aedit.arm64 aedit.x86_64 -create -output aedit

	If ( $LastExitCode -ne 0 )
	{
		Exit $LastExitCode
	}

	@( 'aedit.arm64', 'aedit.x86_64' ) | ForEach-Object {
		Remove-Item -LiteralPath $_
	}

	& make dist

	If ( $LastExitCode -ne 0 )
	{
		Exit $LastExitCode
	}
}

if ($IsWindows -or ( 'Desktop' -eq $PSEdition ))
{
	foreach ($EDITION in 'Community', 'Professional')
	{
		$VCVARSDIR = "${Env:ProgramFiles}\Microsoft Visual Studio\2022\$EDITION\VC\Auxiliary\Build"

		if ( Test-Path -LiteralPath $VCVARSDIR -PathType Container )
		{
			break
		}
	}

	$VCVARSARM = 'vcvarsarm.bat'
	$VCVARSARM64 = 'vcvarsarm64.bat'
	$VCVARSAMD64 = 'vcvars64.bat'
	$VCVARSX86 = 'vcvars32.bat'
	$VCVARSHOST = 'vcvars32.bat'

	switch ($Env:PROCESSOR_ARCHITECTURE)
	{
		'AMD64' {
			$VCVARSX86 = 'vcvarsamd64_x86.bat'
			$VCVARSARM = 'vcvarsamd64_arm.bat'
			$VCVARSARM64 = 'vcvarsamd64_arm64.bat'
			$VCVARSHOST = $VCVARSAMD64
			}
		'ARM64' {
			$VCVARSX86 = 'vcvarsarm64_x86.bat'
			$VCVARSARM = 'vcvarsarm64_arm.bat'
			$VCVARSAMD64 = 'vcvarsarm64_amd64.bat'
			$VCVARSHOST = $VCVARSARM64
		}
		'X86' {
			$VCVARSXARM64 = 'vcvarsx86_arm64.bat'
			$VCVARSARM = 'vcvarsx86_arm.bat'
			$VCVARSAMD64 = 'vcvarsx86_amd64.bat'
		}
		Default {
			throw "Unknown architecture $Env:PROCESSOR_ARCHITECTURE"
		}
	}

	$VCVARSARCH = @{'arm' = $VCVARSARM; 'arm64' = $VCVARSARM64; 'x86' = $VCVARSX86; 'x64' = $VCVARSAMD64}

	$ARCHLIST = ( $VCVARSARCH.Keys | ForEach-Object {
		$VCVARS = $VCVARSARCH[$_];
		if ( Test-Path -LiteralPath "$VCVARSDIR/$VCVARS" -PathType Leaf )
		{
			$_
		}
	} | Sort-Object )

	$ARCHLIST | ForEach-Object {
		New-Object PSObject -Property @{
			Architecture=$_;
			Environment=$VCVARSARCH[$_]
		}
	} | Format-Table -Property Architecture,'Environment'

	Push-Location 'win32'

	$Win32Dir = $PWD

	foreach ($DIR in 'obj', 'bin', 'bundle')
	{
		if (Test-Path -LiteralPath $DIR)
		{
			Remove-Item -LiteralPath $DIR -Force -Recurse
		}
	}

	try
	{
		$ARCHLIST | ForEach-Object {
			$ARCH = $_

			$VCVARS = ( '{0}\{1}' -f $VCVARSDIR, $VCVARSARCH[$ARCH] )

			$VersionStr4="$Version.0"
			$VersionInt4=$VersionStr4.Replace(".",",")

			$xmlDoc = [System.Xml.XmlDocument](Get-Content "Package.appxmanifest")

			$nsMgr = New-Object -TypeName System.Xml.XmlNamespaceManager -ArgumentList $xmlDoc.NameTable

			$nsmgr.AddNamespace("man", "http://schemas.microsoft.com/appx/manifest/foundation/windows10")

			$xmlNode = $xmlDoc.SelectSingleNode("/man:Package/man:Identity", $nsmgr)

			$xmlNode.ProcessorArchitecture = "$ARCH"
			$xmlNode.Version = $VersionStr4

			$xmlDoc.Save("$Win32Dir\AppxManifest.xml")

			@"
CALL "$VCVARS"
IF ERRORLEVEL 1 EXIT %ERRORLEVEL%
NMAKE /NOLOGO clean
IF ERRORLEVEL 1 EXIT %ERRORLEVEL%
NMAKE /NOLOGO DEPVERS_aedit_STR4="$VersionStr4" DEPVERS_aedit_INT4="$VersionInt4" CertificateThumbprint="$CertificateThumbprint" BundleThumbprint="$BundleThumbprint"
EXIT %ERRORLEVEL%
"@ | & "$env:COMSPEC"

			if ($LastExitCode -ne 0)
			{
				exit $LastExitCode
			}
		}

		@"
CALL "$VCVARSDIR\$VCVARSHOST"
IF ERRORLEVEL 1 EXIT %ERRORLEVEL%
NMAKE /NOLOGO DEPVERS_aedit_STR4="$VersionStr4" DEPVERS_aedit_INT4="$VersionInt4" CertificateThumbprint="$CertificateThumbprint" BundleThumbprint="$BundleThumbprint" "aedit-$VersionStr4.msixbundle"
EXIT %ERRORLEVEL%
"@ | & "$env:COMSPEC"

		if ($LastExitCode -ne 0)
		{
			exit $LastExitCode
		}

		Push-Location 'bin'

		try
		{
			Compress-Archive $ARCHLIST -DestinationPath "..\aedit-$Version-win.zip" -Force

			$ARCHLIST | ForEach-Object {
				$ARCH = $_
				$VCVARS = ( '{0}\{1}' -f $VCVARSDIR, $VCVARSARCH[$ARCH] )
				$EXE = "$ARCH\aedit.exe"

				$MACHINE = ( @"
@CALL "$VCVARS" > NUL:
IF ERRORLEVEL 1 EXIT %ERRORLEVEL%
dumpbin /headers $EXE
IF ERRORLEVEL 1 EXIT %ERRORLEVEL%
EXIT %ERRORLEVEL%
"@ | & "$env:COMSPEC" /nologo /Q | Select-String -Pattern " machine " )

				$MACHINE = $MACHINE.ToString().Trim()

				$MACHINE = $MACHINE.Substring($MACHINE.LastIndexOf(' ')+1)

				New-Object PSObject -Property @{
					Architecture=$ARCH;
					Executable=$EXE;
					Machine=$MACHINE;
					FileVersion=(Get-Item $EXE).VersionInfo.FileVersion;
					ProductVersion=(Get-Item $EXE).VersionInfo.ProductVersion;
					FileDescription=(Get-Item $EXE).VersionInfo.FileDescription
				}
			} | Format-Table -Property Architecture, Executable, Machine, FileVersion, ProductVersion, FileDescription
		}
		finally
		{
			Pop-Location
		}
	}
	finally
	{
		Pop-Location
	}
}
