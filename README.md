# DotNetInjector

C++ injector for injecting managed code into unmanaged applications.

Capable of injecting both .Net Framework 4 and modern .NET 6/7/8 code.  Takes several command line arguments:

Inject.exe 
  -m "EntryPoint"                                 # The entry method of the injected application - Must be in the format of 'static int NAME(String pwzArgument)'
    -i "\PATH\TO\CoreInjectionExample.dll"        # The full path to the .dll (If .NET) or .exe (.Net Framework) for injecting
      -l "CoreInjectionExample.Program"           # The namespace and type containing the entrypoint
        -n "xyz.exe"                              # The full .exe name of the target application
          -c "\PATH\TO\netcore"                   # (.NET CORE ONLY, LEAVE BLANK IF FRAMEWORK), the full path to the appropriate .NET CLR runtime folder (Containing coreclr.dll). See below for download information.
            -a ""                                 # Optional arguments, include but leave blank.

# Runtime download

If running .Net framework, the CLR should be bundled with modern Windows and will be detected automatically.
If running a newer .NET app, the injector requires a path to the CLR. These can be downloaded from https://dotnet.microsoft.com/en-us/download/dotnet 


- Choose version
- Scroll down to bottom right '.NET Runtime X.X.XX'
- Click download next to the x64 or x86 binaries
- Open up the .rar - Shared\Microsoft.NETCore.App\X.X.XX\
- Drop all files in this folder somewhere, this is the folder you need to point the -c argument at.
