# V1 最终发布检查清单

## 1. 版本与文档

- [ ] 版本号确认无误
- [ ] `README.md` 与当前代码状态一致
- [ ] `docs/04-v1-protocol.md` 已覆盖当前实际协议方法
- [ ] `docs/07-v1-build-run-test-guide.md` 可直接指导构建与验证
- [ ] `docs/08-v1-release-and-deployment.md` 可直接指导打包与部署
- [ ] `docs/09-v1-validation-report.md` 已更新为本次候选版本的实际结果
- [ ] `docs/10-v1-release-checklist.md` 本身未过期

## 2. Linux / POSIX 验证

- [ ] `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
- [ ] `cmake --build build --parallel 1`
- [ ] `ctest --test-dir build --output-on-failure`
- [ ] `./scripts/validate_v1.sh --build-dir build --config Release --jobs 1`
- [ ] POSIX runtime / audit / history / preview / backup 链路验证通过
- [ ] POSIX install tree 检查通过
- [ ] POSIX archive + native installer 与 checksum 生成通过

## 3. Windows 验证

- [ ] `cmake -S . -B build`
- [ ] `cmake --build build --config Release`
- [ ] `ctest --test-dir build -C Release --output-on-failure`
- [ ] `pwsh ./scripts/windows_smoke.ps1 -BuildDir ./build -Config Release`
- [ ] `pwsh ./scripts/validate_v1.ps1 -BuildDir build -Config Release -Jobs 1`
- [ ] 检查 `.p6_validation_summary/windows_validation_summary.json` 中所有阶段 `ok=true`
- [ ] 检查 `.p6_validation_summary/windows_validation_summary.md` 中 install checks / artifacts 摘要
- [ ] Unicode / 空格路径 / backslash path normalization 验证通过

## 4. 文本编辑与协议能力

- [ ] `fs.write` / `fs.mkdir` 行为与 CLI 参数一致
- [ ] `fs.move` / `fs.copy` / `fs.rename` 行为、覆盖策略与路径限制符合预期
- [ ] `search.text` / `search.regex` 验证通过
- [ ] `request.cancel` / `timeout_ms` / stream final summary 验证通过
- [ ] `patch.preview` / `patch.apply` / `patch.rollback` / `history.list` 验证通过

## 5. 打包与发布链路

- [ ] POSIX 打包：`./scripts/package_release.sh --build-dir build --out-dir dist --jobs 1`（应产出 archive + native installer）
- [ ] Windows 打包：`pwsh ./scripts/package_release.ps1 -BuildDir build -Config Release -OutDir dist -Jobs 1`（应产出 ZIP + NSIS installer）
- [ ] 本地打包输出已生成 `SHA256SUMS.txt` 并校验
- [ ] GitHub Release 资产中的平台校验文件名无冲突（如 `SHA256SUMS-linux.txt` / `SHA256SUMS-windows.txt`）
- [ ] `release.yml` 手工触发时 checkout 的 `tag/ref` 与发布目标一致

## 6. 产物检查

- [ ] 安装目录包含 `bin/bridge_daemon`
- [ ] 安装目录包含 `bin/bridge_cli`
- [ ] 安装目录包含 `share/ai_bridge/README.md`
- [ ] 安装目录仅包含运行时文件（无内部 docs/scripts/include）
- [ ] 归档可正常解压，最小 smoke 可运行

## 7. 风险签收

- [ ] 当前剩余风险已记录在文档中
- [ ] 当前已知限制已可接受
- [ ] 本次候选版本没有阻塞级 crash / 数据损坏问题

## 8. 最终放行

- [ ] 校验文件已归档保存
- [ ] 已记录 Windows 与 Linux 的最终验证结果
- [ ] 允许进入 V1 发布
