# Terms of Use & Online Services Agreement

**Version 1.0 — Effective Date: September 2026**

Welcome to the **SwordigoDesktop** ecosystem. This Terms of Use Agreement ("Agreement" or "Terms") governs your access to and use of the **SwordigoDesktop** desktop client, the **Swordfare Launcher**, the **Ruby GG Studio IDE**, and the **Online Mod Store and Mod Management Network** ("Online Services").

By downloading, installing, launching the software, or connecting to the Online Services, you agree to be bound by these Terms. If you do not agree, do not use the software or connect to the online infrastructure.

---

## 1. Operating Entities & Service Providers

The software and its associated network infrastructure are provided collaboratively by two independent groups:

1. **AevoraLabs (prev OpenSwordigo)**: Stewards of the client applications, build toolchains, Ruby GG Studio IDE, and desktop launcher codebase (see [Project Governance](./GOVERNANCE.md)).
2. **Lawncher Team**: Sole operators, administrators, and hosts of the **online infrastructure**, including the Mod Store API, mod content delivery networks (CDN), remote asset indexing, and user profile management services (see [Project Governance](./GOVERNANCE.md)).

---

## 2. Online Services & Server Infrastructure

### 2.1 "As-Is" and "As-Available" Provision
The Online Services (including browsing, searching, downloading, rating, and uploading community modifications via the Mod Store) are provided as a community courtesy on an **"AS-IS" and "AS-AVAILABLE"** basis, free of charge. Neither the Lawncher Team nor AevoraLabs guarantees:
* Uninterrupted, timely, secure, or error-free server availability;
* Permanent retention of uploaded mods, comments, ratings, or profile statistics; or
* Continued operation of any specific API endpoint or backend protocol.

### 2.2 Capacity Management & Rate Limiting
The Lawncher Team reserves the absolute right to:
* Enforce request rate limits, connection quotas, or bandwidth limits on any user or IP address;
* Schedule maintenance, updates, or emergency downtimes without prior notification; and
* Restrict, modify, or permanently discontinue any feature of the Online Services at its sole discretion.

---

## 3. Acceptable Use Policy & Network Integrity

You agree that you will not engage in any activity that abuses, disrupts, or compromises the online infrastructure. Specifically prohibited activities include:

1. **Automated Scraping & Flooding**: Utilizing automated scripts, scrapers, bots, or crawlers to flood the Mod Store API, brute-force endpoints, or extract catalog databases without authorization.
2. **Denial of Service (DoS/DDoS)**: Attempting to overwhelm, degrade, or attack server resources, CDN endpoints, or backend databases.
3. **Authentication & Protocol Tampering**: Exploiting, probing, or attempting to bypass security tokens, authentication headers, API verification hashes, or account controls.
4. **Banned User Evasion**: Creating alternative accounts or utilizing proxy rotation to circumvent server bans or IP blacklists.

**Violations of this section will result in immediate and permanent revocation of access to the Online Services, including automated IP and hardware-level bans.**

---

## 4. Mod Store & User-Generated Content (UGC)

The Mod Store enables community members to share and download user-generated modifications. Both mod creators and downloading players agree to the following rules:

### 4.1 Prohibited Content
Any mod uploaded to the Mod Store must strictly adhere to clean, legal community standards. Prohibited content includes without limitation:
* **Malware & Harmful Payloads**: Mods containing trojans, backdoors, spyware, cryptominers, ransomware, keyloggers, or arbitrary remote code execution exploits.
* **Commercial Game Piracy**: Mods must **never** distribute Touch Foo’s original copyrighted commercial game binaries (`libswordigo.so`), full proprietary game packages, or unlicensed commercial assets. Mods must take the form of original code, delta patches, custom meshes, or procedural assets that require the user to own a legitimate copy of the base game.
* **Harassment & Hate Speech**: Mods, descriptions, or titles containing hate speech, harassment, threats, or explicit adult content (in violation of our [Code of Conduct](./CODE_OF_CONDUCT.md)).

### 4.2 Hosting License Granted by Mod Authors
When you upload or submit a mod to the Mod Store, you retain your underlying copyright in your original creation (consistent with our [CLA](./CLA.md)):
* You grant the **Lawncher Team** and **AevoraLabs (prev OpenSwordigo)** a non-exclusive, worldwide, royalty-free, perpetual license to **host, cache, index, re-encode, display, and distribute** your mod and its metadata to end users through the launcher, game overlay, and API.
* You warrant that you are the author of the mod or possess sufficient legal authorization to publish it.

### 4.3 Takedown & Content Moderation Authority
The Lawncher Team and AevoraLabs reserve the right, but assume no obligation, to:
* Review, inspect, scan, or analyze any uploaded mod for malicious behavior; and
* Edit, delist, unpublish, or permanently delete any mod from the store at any time, for any reason, including upon receipt of copyright infringement notices or safety concerns, without prior liability to the mod author.

---

## 5. End-User Safety & Limitation of Liability

### 5.1 Use at Your Own Risk
Community mods execute custom code within the Swordigo runtime environment. You acknowledge and agree that:
* Neither AevoraLabs nor Lawncher Team independently validates or guarantees the safety, stability, balance, or bug-free nature of any community mod.
* **Downloading and running mods is done entirely at your own risk.**

### 5.2 Disclaimer of Damages
Under no circumstances shall AevoraLabs, the Lawncher Team, or individual project contributors be liable for:
* Corruption or loss of game save files, player profiles, or character progress;
* Application crashes, memory leaks, performance degradation, or graphics corruption caused by third-party mods; or
* Direct, indirect, incidental, special, or consequential damages resulting from the use of or inability to use the software or Online Services.

> **Recommendation**: Players are strongly advised to regularly back up their game save files before installing or updating community mods.

---

## 6. Trademarks & Legal Disclaimers

* *Swordigo* is a registered trademark of Touch Foo / Ville Mäkynen.
* SwordigoDesktop, the Ruby GG Studio IDE, Swordfare, and the Lawncher Mod Store are independent community-driven fan projects and are **not** endorsed by, affiliated with, or sponsored by Touch Foo.
* All original trademarks, logos, and game assets remain the property of their respective owners.

---

## 7. Changes to these Terms

We reserve the right to revise these Terms of Use at any time. Updated versions will be published in the project repository with a revised effective date. Your continued use of the software or Online Services after any modification constitutes your acceptance of the revised Terms.

---

## 8. Contact & Reporting

To report security vulnerabilities, abusive network behavior, or intellectual property concerns regarding content on the Mod Store, please contact the AevoraLabs and Lawncher Team administrators via official project channels.

---

## 9. Related Community Policies

* **[Contributor License Agreement (CLA)](./CLA.md)**: Author copyright and contribution terms.
* **[Project Governance Model](./GOVERNANCE.md)**: Decision procedures and organization structure.
* **[Contributing Guidelines](./CONTRIBUTING.md)**: Technical standards and pull request workflows.
* **[Code of Conduct](./CODE_OF_CONDUCT.md)**: Community standards and harassment policy.
