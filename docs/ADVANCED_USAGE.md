# 高级使用方式

## 上海海洋大学默认登录

新配置及“恢复默认”使用 `vpn.shou.edu.cn:443` 的 aTrust 统一身份认证（CAS），登录域为 `cas07533`，入口为 `/passport/v1/public/casLogin?sfDomain=cas07533`。连接时在内嵌窗口完成学校统一身份认证，程序自动接收回调；服务器要求时继续完成 TOTP 或验证码验证。

升级至配置版本 10 时，程序只迁移仍匹配旧学校默认值的配置：服务器及端口未更改、EasyConnect 密码认证、未配置证书、认证方式/登录域/登录入口为空。仅更新协议和 CAS 设置，保留账号、密码、TOTP、代理及 DNS。迁移前会在配置文件旁保存 `.pre-v10.bak` 备份；备份失败或存在不同内容的同名备份时，保留旧配置并提示。其他配置保持原值。服务器调整认证方式后，可在设置中重新获取认证方式。

## TOTP 二次认证

在“设置 → 通用”的“TOTP 密钥”中填写验证器密钥，可由核心自动生成验证码；留空时，服务器要求 TOTP 验证后会弹出输入框，填写当次验证码即可。配置引导的凭据步骤也提供同一个可选设置，适用于 CAS、OAuth2、短信、账号密码及证书登录后的 TOTP 验证。

每个连接配置保存一份 TOTP 密钥，在该配置的 aTrust 与 EasyConnect 模式之间共用。切换模式会保留密钥，不同连接配置之间互不影响。此处填写的是验证器密钥，而非当次验证码；清空密钥并保存即可恢复手动输入。

## Clash / Mihomo 分流配置

若需要使用 Clash、Mihomo 等其他代理软件与本软件配合分流，可以采用以下两种方式之一：

1. **本软件作为主代理**：开启本软件的系统代理功能，将本软件认为不需要代理的流量通过“直连代理”转发给 Clash 或 Mihomo。
2. **Clash/Mihomo 作为主代理**：关闭本软件的系统代理功能，将需要本软件代理的流量转发到本软件。

以下是具体操作流程：

### 方式一：本软件作为主代理

- 在“功能”-“设置”-“高级” 页面中设置“直连代理”为 Clash 等软件监听的端口（以 7890 为例 http://127.0.0.1:7890）；
- 注意勾选“允许外部访问”；
- 开启本软件的系统代理；
- 启动 Clash 时不需要打开系统代理或 TUN，即可正确转发流量。

<div align="center">
<img src="proxy_setting.png" width="600px">
</div>

### 方式二：Clash 作为主代理

- 清空本软件的系统代理；
- 在 “功能”-“设置”-“高级” 页面中设置 SOCK5 的代理端口（以 11080 为例），以下为推荐配置：

在 Clash 的代理配置中添加一个代理服务器：

```yaml
# 代理服务器
proxies:
  - name: 🖥 SHOUConnect
    type: socks5
    server: 127.0.0.1
    port: 11080
    udp: true
```

并在代理组中添加一个单独的代理组：

```yaml
proxy-groups:
  - name: "🏫 校园网"
    type: select
    proxies:
      - DIRECT
      - 🖥 SHOUConnect
```

并在规则中加入：

```yaml
rules:
  - DOMAIN,ids.hit.edu.cn,DIRECT      # 鉴权服务器
  - DOMAIN,trust.hitsz.edu.cn,DIRECT  # aTrust 服务器
  - DOMAIN-SUFFIX,hitsz.edu.cn,🏫 校园网
  - IP-CIDR,10.0.0.0/8,🏫 校园网,no-resolve
  # 可在此添加其它你需要代理的 ip 段，如课程中心
```

这样即可通过简单的切换实现在校外使用本项目时选择 SHOUConnect 代理，在校内使用 DIRECT 直连。

<div align="center">
<img src="proxy_group.png" width="600px">
</div>

## TUN 模式

### Clash 作为主代理

在本方式中，Clash 提供 TUN 虚拟网卡服务，捕获全部流量并将预筛选符合条件的流量发送给 SHOUConnect 代理。

SHOUConnect 收到的流量会经过内部分流，将软件需要代理流量（默认为校园网流量）送入 VPN 通道，其余流量直接放行。

**这里需要特别注意**，在 Clash TUN 网卡的作用下，SHOUConnect 送出的流量会再次回到 Clash，因此本方式中**务必设置规则以排除这部分流量**，防止其再次被代理送回 SHOUConnect 引起回环。

1. 取消/清空本软件的系统代理，无需设置“直连代理”，注意勾选“允许外部访问”；
2. 在 Clash 中配置 TUN 相关设置。

推荐配置方式如下：

在 Clash 的代理配置中添加一个代理服务器：

```yaml
# 代理服务器
proxies:
  - name: 🖥 SHOUConnect
    type: socks5
    server: 127.0.0.1
    port: 11080
    udp: true
```

并在代理组中添加一个单独的代理组：

```yaml
proxy-groups:
  - name: "🏫 校园网"
    type: select
    proxies:
      - DIRECT
      - 🖥 SHOUConnect
```

并在规则中加入：

```yaml
rules:
  - DOMAIN,ids.hit.edu.cn,DIRECT      # 鉴权服务器
  - DOMAIN,trust.hitsz.edu.cn,DIRECT  # aTrust 服务器
  - PROCESS-NAME,zju-connect.exe,DIRECT
  - PROCESS-NAME,SHOUConnect.exe,DIRECT
  - DOMAIN-SUFFIX,hitsz.edu.cn,🏫 校园网
  - IP-CIDR,10.0.0.0/8,🏫 校园网,no-resolve
  # 可在此添加其它你需要代理的 ip 段，如课程中心
```

其中：

- `PROCESS-NAME`精确匹配`SHOUConnect.exe`和`zju-connect.exe`联网核心进程；
- 上述两类规则在可以正确匹配的情况下选其一或保留两者均可，推荐使用`PROCESS-NAME`。并且**必须**至少放在`DOMAIN-SUFFIX,hitsz.edu.cn,🏫 校园网`之前，以达到放行流量，防止回环的目的。
- 在较新版本的 SHOUConnect 中，可以通过设置“自动检测网口“来避免回环，此时可以不使用`PROCESS-NAME`规则。


最后，还需要在 DNS 配置中为`fake-ip`添加过滤规则，防止 SHOUConnect 的域名解析到 fake-ip 地址，从而无法正确分流。

```yaml
dns:
  # 仅在使用 fake-ip 时需要配置
  enhanced-mode: fake-ip
  fake-ip-filter:
    - +.hitsz.edu.cn
```

如果使用 Clash 的“全局扩展脚本”功能动态修改配置，以下是最小示例，可自行对比修改：

```javascript
// Define main function (script entry)

// DNS 配置
const dnsConfig = {
	"enhanced-mode": "fake-ip",
	"fake-ip-filter": ["+.hitsz.edu.cn"],
};

function main(config, profileName) {
	// 使用先前定义的 DNS 设置（会覆盖整个 dns 对象）
	// 如果希望增量修改，请自行重写
	config["dns"] = dnsConfig;

	// 校园网
	config.proxies = config.proxies || [];
	config["proxy-groups"] = config["proxy-groups"] || [];
	config.rules = config.rules || [];

	config.proxies.push({
		name: "SHOUConnect",
		type: "socks5",
		server: "127.0.0.1",
		port: 11080,
		udp: true,
	});

	config["proxy-groups"].push({
		name: "校园网",
		type: "select",
		proxies: ["DIRECT", "SHOUConnect"],
	});

	config.rules.unshift(
		"DOMAIN,trust.hitsz.edu.cn,DIRECT",
		"PROCESS-NAME,zju-connect.exe,DIRECT",
		"PROCESS-NAME,SHOUConnect.exe,DIRECT",
		"DOMAIN-SUFFIX,hitsz.edu.cn, 校园网",
		"IP-CIDR,10.0.0.0/8, 校园网,no-resolve",
	);

	// 返回修改好的配置
	return config;
}
```

额外注意，`tun`的`route-exclude-address`（默认为空）不应加入`10.0.0.0/8`网段，否则会导致流量不能正确经由 tun 转发到 SHOUConnect。
