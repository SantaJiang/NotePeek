# NotePeek

## 各平台打包说明

### Windows 打包:
```bash
# 构建项目
qmake MainPage.pro
make release

# 打包
make package    # 创建包含所有依赖的文件夹
make zip        # 创建ZIP分发包
make installer  # 创建安装程序
```

### macOS 打包:
```bash
# 构建项目
qmake MainPage.pro
make release

# 打包
make dmg        # 创建DMG磁盘映像
```

### Linux 打包:
```bash
# 构建项目
qmake MainPage.pro
make release

# 打包
make appimage   # 创建AppImage
```