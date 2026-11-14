![Phobos YR Engine Extension](logo.png)

[![Github All Releases](https://img.shields.io/github/downloads/Phobos-developers/Phobos/total.svg)](https://github.com/Phobos-developers/Phobos/releases)
[![Docs status](https://readthedocs.org/projects/phobos/badge/?version=latest)](https://phobos.readthedocs.io/en/latest/?badge=latest)
[![Workflow](https://img.shields.io/github/actions/workflow/status/Phobos-developers/Phobos/nightly.yml?branch=develop)](https://github.com/Phobos-developers/Phobos/actions)
[![EditorConfig](https://github.com/Phobos-developers/Phobos/workflows/EditorConfig/badge.svg)](https://github.com/Phobos-developers/Phobos/actions?query=workflow%3AEditorConfig)
[![license](https://img.shields.io/github/license/Phobos-developers/Phobos.svg)](https://www.gnu.org/licenses/gpl-3.0.html)

> **Warning**
> The project is currently not maintained actively enough and thus we are looking for active maintainers at the moment. Please message us [in Discord channel](https://discord.gg/sZeMzz6qVg) (or PM Kerbiter directly).

# 额外功能说明

> - 本平台为Phobos-Recya的特殊分支版本（Recloud）

> - Modified by **[CrimRecya](https://github.com/CrimRecya)**
> - Phobos-Recya平台仓库：**[Phobos-Mix / Mix-ECpack](https://github.com/CrimRecya/Phobos-Mix/tree/Mix-ECpack)**
> - 本分支仓库：**[Phobos-Recya](https://github.com/wenrui1245/Phobos-Recya)**
> - 公版地址：**[Phobos 官方文档](https://phobos.readthedocs.io/en/latest/#)**
> - 公版仓库：**[Phobos-developers/Phobos](https://github.com/Phobos-developers/Phobos)**

> [!TIP]
> 在正式使用前，请认真阅读以下使用须知

---

## 使用须知

- 本平台仅在**凛冬将至**、**Kaleidoscope**群内发布最新版本，并授权EC整合包开发组合使用
- 除制作任务包、模组演示外，**不允许**以任何形式在未获得作者许可的情况下，对本平台一个或多个独有功能进行独立展示
- 本平台仅作内部测试使用，不对外宣传，除待公版合并的内容外，**不允许**任何人以任何形式直接对外宣传平台开发内容
  > 为什么不对外宣传？-本质是测试，其功能并不如公版稳定，bug发出去了没人修
- 本平台可以在上述条件下自行使用，但请注意公开场合使用时（直播、视频等），必须在合适位置注明使用的本平台**并非公版**
- 如果需要反馈问题，请在确认为**最新版本**后，在群内联系作者以寻求解决方案
- 如只需在模组使用部分功能，请自行合并需要的内容，并**标明来源**
  > 严禁冒名顶替作者的行为，请尊重他人的劳动成果
- 平台试用期自发布起计算，共3653天，自用时请自行更新最新版本，若需要发布任务包、模组使用，请联系作者
- 本分支的维护采用了Vibe Coding，请自行评估稳定性后再使用。
- 本分支属于Phobos-Recya的续更版，并不受到主分支作者绯红热茶、航味麻酱的支持。

> [!NOTE]
> **`注意`** 平台并非所谓新版、开发版Phobos
> - 就目的而言，平台属于测试版，其中部分功能*可能*会被公版合并
> - 就结果而言，平台属于私改版，与公版并无直接关系

> [!NOTE]
> **`注意`** 凛冬将至（WIC/486835964）群并非本平台作者的群，仅作为传统在其中进行发布
> - 凛冬将至群为WIC平台/模组的官方群，请保证基本的礼貌和尊重
> - 本平台不会与WIC平台发生直接功能冲突，但如在游戏过程中发现此类问题，请及时反馈

---

## 温馨提醒

- 热茶因为现实原因不再维护Phobos-Recya，并把项目移交给了麻酱管理。但更新速度仍然非常慢，所以我（凌风之云awa）开了一个新的分支，基于Phobos-Recya2，并更名为Phobos-Recloud。
- 本分支的开发有三个目的：积极跟进公版Phobos的更新、为Kaleidoscope平台提供接口、增加新功能。

> [!TIP]
> 如果在刚更换完dll时发生问题，请先尝试重启电脑，可能是dll未成功完全完成替换导致的

---

## 特别感谢

- 感谢 **`Phobos`** 的所有开发者和贡献者
- 感谢 **`Sidoupiar`** 给予的额外功能开发方面的帮助
- 感谢 **`钢铁之锤`** 制作了适配的EC开发组件
- 感谢 **`航味麻酱`** 制作了更多的拓展功能并帮助维护平台
- 感谢 **`凛冬将至`** 的**群友们**给予的思路并帮忙进行各种测试
- 感谢 **`pbs测试群`** 的**群友们**给予的思路并帮忙进行各种测试
- 感谢 **`绯红热茶`** 开发Phobos-Recya

---

## 内容简介

- 公版内容不做任何解释，请自行前往 **[https://phobos.readthedocs.io/en/latest/](https://phobos.readthedocs.io/en/latest/)** 查阅公版最新的英文说明书
  > **[https://phobos.readthedocs.io/zh-cn/latest/](https://phobos.readthedocs.io/zh-cn/latest/)** 为每周更新，会略慢于英文版本
- 注意使用的是develop分支而不是build（名称中的build只是简单告诉你现在是在哪个build之后的版本），即公版部分内容也是随版本更新的，具体更新的内容可以根据《更新改动说明》中的commit编号进行搜索
  > 公版问题可以提出，作者会尝试为公版问题提供修复
- 其它 **Phobos Contributor** 制作的未合并功能，内容来自于 **[Phobos Pull Requests](https://github.com/Phobos-developers/Phobos/pulls)**
  > 出现问题时不保修，出现严重问题时请联系我，我会将其剔除；当该这些内容被公版合并时会从本说明书中移除

- **[CrimRecya](https://github.com/CrimRecya)** ，平台作者
- **[TaranDahl](https://github.com/TaranDahl)** ，主要功能作者
- **[wenrui1245](https://github.com/wenrui1245)** ，本分支维护者

---

# Phobos

...is a community engine extension project providing a set of new features and fixes for Yuri's Revenge based on [modified YRpp](https://github.com/Phobos-developers/YRpp) and [SyringeEx](https://github.com/Phobos-developers/SyringeEx) to allow injecting code. It's meant to accompany [Ares](https://github.com/Ares-Developers/Ares) rather than replace it, thus it won't introduce incompatibilities.

While Phobos is independent of Ares and does NOT require Ares specifically to function, Phobos complements some of the features found in Ares and vice versa.

EA has not endorsed and does not support this product.

Community
---------

As the project is international and English is currently studied the most commonly as a second language, it's the main project language. We do not limit the creation of language-specific community groups though, because we understand that many may not know English as well as their native language and/or may have trouble accessing Discord.

- **[🌐 International Discord channel on C&C Mod Haven](https://discord.gg/sZeMzz6qVg)**
- **[🇨🇳 Chinese Phobos Communication Forum - Phobos CN](https://phoboscn.top)**

Downloads
---------

You can choose one of the following:
- [Latest stable branch build](https://github.com/Phobos-developers/Phobos/releases/latest) (most bug-free release but very slow on new features)
- [Latest pre-release builds](https://github.com/Phobos-developers/Phobos/releases) (a bit less bug-free releases, pre-releases get new features when they are finished)
- [Latest development branch nightly](https://nightly.link/Phobos-developers/Phobos/blob/develop/.github/workflows/nightly.yml) (added unreleased features that will be in the next pre-release)
- Individual new feature nightly builds for testing can be found in [pull requests](https://github.com/Phobos-developers/Phobos/pulls)

To learn how these build types relate to each other and how versioning works, see the [release model and version lifecycle](docs/Project-guidelines-and-policies.md#git-branching-model-version-lifecycle-and-release-strategy).

### Note on nightly builds

Last two listed versions are bleeding edge (don't redistribute them outside of testing!) and have build information (commit and branch/tag) in them which is displayed ingame and can't be turned off. You can get a build for development branch (link above) any up-to-date pull request via an automatic bot comment that would appear in it and would contain the most recent successfully compiled version of Phobos for that feature branch. Please note that the build is  produced *only if the PR has no merge conflicts*. Alternatively, you can get an artifact manually from GitHub Actions runs. You can get an artifact for a specific commit which is built automatically with a GitHub Actions workflow, just press on a green tick, open the workflow, find and download the build artifact. This is limited to authorized users only.

Installation and Usage
----------------------

0. Phobos requires [SyringeEx](https://github.com/Phobos-developers/SyringeEx) (v0.1.0.2 or newer) - an extended, open-source version of Syringe - to run; the game will show an error and quit on startup under older Syringe versions. Phobos packages and nightly builds come with the SyringeEx `Syringe.exe` bundled; it can also be downloaded separately from the [SyringeEx releases page](https://github.com/Phobos-developers/SyringeEx/releases). It's highly recommended to **install [Ares](https://launchpad.net/ares/+download)** too to get full Phobos feature set, just drop all the files from the archive except documentation folder and `Syringe.exe` into your game folder.
1. Obtain a Phobos "package" (official builds can be found on [releases page](https://github.com/Phobos-developers/Phobos/releases); read below to learn how to get nightly builds). You should end up with `Phobos.dll`, `Phobos.pdb` and the bundled SyringeEx `Syringe.exe`.
2. Place those files in the game folder (where your `gamemd.exe` is located), replacing any existing `Syringe.exe` (for example the one shipped with Ares).
3. To launch the game with Phobos (and all other installed Syringe-compatible engine extensions including Ares) you need to execute `Syringe.exe "gamemd.exe" [command line arguments for gamemd.exe]` in command line (omit arguments if you don't need any). `RunAres.bat` from Ares package does the same so you may use that as well.

Be sure to read [migration and breaking changes](docs/Whats-New.md#migration-breaking-changes) to know if you need to adjust something in your mod after Phobos installation (or update).

If you already use Ares in your mod, you just need to drop Phobos files mentioned above in your game folder, Syringe will load Phobos automatically. This also applies to mods using XNA client with Syringe; if your mod doesn't use Syringe and Ares (or you just haven't set up the client) yet we recommend to use [CnCNet client mod base by Starkku](https://github.com/Starkku/cncnet-client-mod-base) which is compatible with Ares and Phobos out of the box.

Additional files and tools that you may need are located at [Phobos supplementaries repo](https://github.com/Phobos-developers/PhobosSupplementaries).

By default Phobos doesn't do any very noticeable changes except a few bugfixes. To learn how to use Phobos features head over to official documentation.

Documentation
-------------

- [Official docs](https://phobos.readthedocs.io) (also available in [Chinese](https://phobos.readthedocs.io/zh_CN/latest))
- [Community Chinese docs](https://docs.qq.com/doc/p/dc3da1ce39a6e787b6e133f7d33d6aebef581cb4)
  - Because the Chinese translation of the official docs is currently underdeveloped, at the time it is recommended to use the community docs for Chinese users.

You can switch between versions (displays latest develop nightly version by default) in the bottom right corner, as well as download a PDF version.

The documentation is split by a few major categories, each represented with a page on the sidebar. Each page has its contents grouped into multiple subcategories, be it buildings, technotypes, infantry, superweapons or something else.

### How to read code snippets

```ini
; which section the entries should be in
; can be a freeform name - in this case the comment would explain what it is
; if no comment to be found - then it's a precise name
[SOMENAME]           ; BuildingType
; KeyName=DefaultValue ; accepted type with optional explanation
; if there's nothing to the right of equals sign - the default value is empty/absent
; if these keys have had their value set, they can only be set to their default
; unset state again by setting the value to <default>, <none> or none
; for list of values only <default> clears the entire list
; if the default value is not static - it's written and explained in a comment
UIDescription=<none> ; CSF entry key
```

Building manually
-----------------

0. Install **Visual Studio** (2022 is minimum) with the dependencies listed in `.vsconfig` (it will prompt you to install missing dependences when you open the project, or you can run VS installer and import the config). If you prefer to use **Visual Studio Code** you may install **VS Build Tools** with the dependencies from `.vsconfig` instead. Not using a code editor or IDE and building via **command line scripts** included with the project is also an option.
1. Clone this repo recursively via your favorite git client (that will also clone YRpp).
2. To build the extension:
   - in Visual Studio: open the solution file in VS and build it (`Debug` build config is recommended);
   - in VSCode: open the project folder and hit `Run Build Task...` (`Ctrl + Shift + B`);
   - barebones: run `scripts/build_debug.bat`.
3. Upon build completion the resulting `Phobos.dll` and `Phobos.pdb` would be placed in the subfolder identical to the name of the buildconfig executed.

Credits
-------

This project was founded by [@Belonit](https://github.com/Belonit) (Gluk-v48) and [@Metadorius](https://github.com/Metadorius) (Kerbiter) in 2020, with the first public stable release in 2021. Since then it has grown into a large community project with many contributors and maintainers.

### Interoperability

Phobos has opened the external interfaces of some key components. If you are also developing your own engine extension and wish to use Phobos at the same time, please check out [Interoperability](docs/Interoperability.md).

### Maintenance crew

Maintenance crew consists of experienced Phobos contributors who are recognized and given the permission to maintain and shape the project to the extent of their permissions.

Every maintenance crew member is welcome to put a donation link to their entry in the list below.

- **Kerbiter ([@Metadorius](https://github.com/Metadorius))** - T3 maintainer (lead)
  - [Patreon](https://www.patreon.com/kerbiter) · PayPal (preferable because of no fees) on request
- **[@Starkku](https://github.com/Starkku)** - T3 maintainer (co-lead)
  - [Patreon](https://www.patreon.com/Starkku)
- **[@CrimRecya](https://github.com/CrimRecya) (绯红热茶)** - T2 maintainer
  - [Alipay](https://www.phoboscn.top/t/topic/45#crimrecya)
- **[@ZivDero](https://github.com/ZivDero)** - T2 maintainer
  - [Patreon](https://www.patreon.com/c/ZivDero)
- **Ollerus ([@Coronia](https://github.com/Coronia))** - T1 maintainer
  - [Alipay](https://www.phoboscn.top/t/topic/45#ollerus)
- **[@NetsuNegi](https://github.com/NetsuNegi)** - T1 maintainer
  - [Alipay](https://www.phoboscn.top/t/topic/45#netsunegi)
- **[@TaranDahl](https://github.com/TaranDahl) (航味麻酱)** - T1 maintainer
  - [WeChatPay](https://www.phoboscn.top/t/topic/45#tarandahl)
- **Noble_Fish ([@DeathFishAtEase](https://github.com/DeathFishAtEase))** - triage, doc maintainer, T1 maintainer
  - [Alipay](https://www.phoboscn.top/t/topic/45#noble_fish)
- **FlyStar ([@Fly-Star-him](https://github.com/Fly-Star-him))** - triage
- **[@Fryone](https://github.com/Fryone)** - triage

#### Inactive

*Please note that being put here just means that you seem to be currently inactive as a part of maintenance crew. You are always welcome to return to the active crew if you want to help out again!*

- **Gluk-v48 ([@Belonit](https://github.com/Belonit))** - lead in the past
- **Uranusian ([@Thrifinesma](https://github.com/Thrifinesma))** - T2 maintainer, CN community ambassador, doc maintainer
- **[@secsome](https://github.com/secsome)** - maintainer
- **[@Otamaa](https://github.com/Otamaa) (Fahroni, BoredEXE)** - maintainer
- **[@FS-21](https://github.com/FS-21)** - inactive as a maintainer specifically
- **Morton ([@MortonPL](https://github.com/FS-21))** - T2 maintainer
- **Trsdy ([@chaserli](https://github.com/chaserli))** - T2 maintainer

The project is so big that listing here all the help we receive or received will make the size of the readme explode, so please see the [full credits list](CREDITS.md) for that. We appreciate your help, contributions and support regardless!

Attribution
-----

You can show your appreciation and help project's publicity by displaying the logo (monochrome version can be found [here](https://github.com/Phobos-developers/Phobos/blob/develop/logo-mono.png)) in your client/launcher (make it a button that opens Phobos GitHub page for extra fanciness). To fit with the mod styling, you are allowed to stylize the monochrome logo in a non-intrusive way (for example, recolor it to match the mod theme) without otherwise changing it. If unsure - ask us first.

When promoting features of your mod that you implemented using Phobos, please give credit to Phobos and it's contributors. A good promotion for Phobos is concise and specific to features that are being showcased, for example: "This feature is made possible by Phobos", "Implemented using XYZ from Phobos", etc. This helps end users understand the nature of Phobos and its role in the modding community, and gives the needed recognition to the project and its contributors.

Legal and License
-----

[![GPL v3](https://www.gnu.org/graphics/gplv3-127x51.png)](https://opensource.org/license/GPL-3.0)

The Phobos project is an unofficial open-source community collaboration project to extend the Red Alert 2 Yuri's Revenge engine for modding and compatibility purposes.

As a modification, the project complies with [EA C&C modding guidelines](https://www.ea.com/games/command-and-conquer/command-and-conquer-remastered/news/modding-faq); should there be conflict between the project's license and modding guidelines - the rules imposed by guidelines shall take precedence (for example, the project should not be commercial or used to make money).

This project has no direct affiliation with Electronic Arts Inc. Command & Conquer, Command & Conquer Red Alert 2, Command & Conquer Yuri's Revenge are registered trademarks of Electronic Arts Inc. All Rights Reserved.
