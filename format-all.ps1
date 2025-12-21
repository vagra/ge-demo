# format.ps1

# ================= 配置区域 =================
# 这里填入 msys64 下 clang-format 的绝对路径
$ClangFormatExe = "D:\Library\msys64\clang64\bin\clang-format.exe"
# ===========================================

# 1. 检查工具是否存在
if (-not (Test-Path $ClangFormatExe)) {
    Write-Host "❌ 错误: 找不到 clang-format 程序: $ClangFormatExe" -ForegroundColor Red; exit 1
}

# 2. 检查配置文件
if (-not (Test-Path ".\.clang-format")) {
    Write-Host "❌ 错误: 当前目录未找到 .clang-format 文件！" -ForegroundColor Red; exit 1
}

Write-Host "🚀 开始格式化..." -ForegroundColor Cyan

# 获取当前脚本运行的根目录路径，用于计算相对路径
$RootPath = (Get-Location).Path
$files = Get-ChildItem -Path . -Recurse -Include *.c,*.h -File

if ($files.Count -eq 0) {
    Write-Host "⚠️  未找到源文件。" -ForegroundColor Yellow; exit 0
}

$modifiedCount = 0

foreach ($file in $files) {
    # 1. 计算相对路径 (把 D:\...\ 去掉，只保留 .\packages\...)
    # Substring 截取掉根目录长度+1个字符(斜杠)
    $relativePath = $file.FullName.Substring($RootPath.Length + 1)

    # 2. 记录当前文件的“最后修改时间”
    $originalTime = $file.LastWriteTime

    # 3. 执行格式化
    & $ClangFormatExe -i -style=file "$($file.FullName)"

    # 4. 【关键】刷新文件对象状态，否则 LastWriteTime 还是旧值
    $file.Refresh()

    # 5. 对比时间戳
    if ($file.LastWriteTime -ne $originalTime) {
        # 时间变了，说明被格式化了
        Write-Host "📝 Modified:  $relativePath" -ForegroundColor Yellow
        $modifiedCount++
    } else {
        # 时间没变，说明格式本来就是对的 (选做：如果是强迫症，可以把下面这行注释掉，只显示修改过的文件)
        Write-Host "✔  Skipped:   $relativePath" -ForegroundColor DarkGray
    }
}

Write-Host "`n--------------------------------------------------" -ForegroundColor Gray
if ($modifiedCount -gt 0) {
    Write-Host "✅ 完成！共扫描 $($files.Count) 个文件，修复了 $modifiedCount 个文件。" -ForegroundColor Green
} else {
    Write-Host "✨ 完美！所有文件的格式已经是正确的了。" -ForegroundColor Green
}