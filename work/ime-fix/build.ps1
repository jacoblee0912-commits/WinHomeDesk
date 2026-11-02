$ErrorActionPreference='Stop'
$base=Split-Path $PSScriptRoot
$desktop='C:/Program Files/dotnet/shared/Microsoft.WindowsDesktop.App/10.0.11'
$refs=@([AppContext]::GetData('TRUSTED_PLATFORM_ASSEMBLIES').Split([IO.Path]::PathSeparator))
$refs+=@('System.Windows.Forms.Primitives.dll','System.Windows.Forms.dll','System.Drawing.Common.dll') | ForEach-Object {Join-Path $desktop $_}
$metadata=[System.Collections.Generic.List[Microsoft.CodeAnalysis.MetadataReference]]::new()
foreach($ref in ($refs | Select-Object -Unique)){$metadata.Add([Microsoft.CodeAnalysis.MetadataReference]::CreateFromFile($ref))}
$syntax=[Microsoft.CodeAnalysis.CSharp.CSharpSyntaxTree]::ParseText([IO.File]::ReadAllText("$PSScriptRoot/StartupHook.cs"))
$options=[Microsoft.CodeAnalysis.CSharp.CSharpCompilationOptions]::new([Microsoft.CodeAnalysis.OutputKind]::DynamicallyLinkedLibrary)
$comp=[Microsoft.CodeAnalysis.CSharp.CSharpCompilation]::Create('WinHomeDesk.ImeHook',[Microsoft.CodeAnalysis.SyntaxTree[]]@($syntax),$metadata,$options)
$stream=[IO.File]::Create("$PSScriptRoot/WinHomeDesk.ImeHook.dll")
try{$result=$comp.Emit($stream);$result.Diagnostics | Where-Object Severity -eq Error | ForEach-Object ToString;if(!$result.Success){throw 'Hook compilation failed'}}finally{$stream.Dispose()}
