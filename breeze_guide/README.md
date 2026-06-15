# Breeze Guide

Breeze Guide 是丧尸生存 Roguelike 游戏 [Cataclysm: Dark Days Ahead](https://cataclysmdda.org/) 的游戏数据指南，已适配 [CDDA-Breeze](https://github.com/breeze-cdda/CDDA-Breeze) 分支。你可以搜索游戏中的物品、怪物、家具等，并查看它们的详细信息。数据来源于游戏本身的 JSON 文件。

指南将数据存储在本地，支持离线使用。只要访问过一次，无需联网即可正常工作。

## 数据

运行 `merge-json.mjs` 脚本可以将 `data/json/` 目录下的所有 JSON 文件合并为单个 `public/data/all.json` 文件。重新生成：

```
node merge-json.mjs
```