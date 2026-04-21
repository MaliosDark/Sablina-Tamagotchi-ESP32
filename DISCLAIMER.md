# ⚠️ Legal Disclaimer & Responsible Use Notice

> **Read this document before building, flashing, or operating any firmware from this repository.**

---

## 1. Purpose & Intended Use

The Sablina / Sablina Tamagotchi ESP32 project is an **open-source educational tool** designed for:

- **Authorised penetration testing** of wireless networks you own or have explicit written permission to test
- **Academic research** into 802.11 security protocols (WPA2 handshake capture, PMKID, deauthentication frame injection)
- **CTF (Capture The Flag) competitions** and controlled lab environments
- **Professional security assessments** where written scope and authorisation are in place

This firmware is provided in the same spirit as tools such as `aircrack-ng`, `hashcat`, `Kismet`, and `Wireshark`, widely used, openly documented, and legal when operated responsibly.

---

## 2. The Pet Works Without WiFi Auditing

**The WiFi security audit module is entirely optional and compiled out by default in stripped builds.**

To disable all offensive radio features, set the following in `SablinaTamagotchi_2.0/config.h`:

```c
#define FEATURE_WIFI_AUDIT   0   // ← disables ALL audit/attack features
```

With `FEATURE_WIFI_AUDIT 0`, the device operates **purely as a virtual pet**, feeding, sleeping, playing, BLE social interaction, and LLM personality, with zero wireless security tooling compiled in. No deauthentication frames are ever transmitted. This is the recommended configuration for daily carry, school/work environments, or any jurisdiction where you are uncertain of local laws.

---

## 3. Applicable Law, United Kingdom

Operating this device with `FEATURE_WIFI_AUDIT 1` on **networks you do not own or lack explicit written authorisation to test** may constitute serious criminal offences under UK law, including but not limited to:

| Legislation | Relevant Provisions |
|---|---|
| **Computer Misuse Act 1990** | §1 (unauthorised access), §3 (unauthorised modification / interference), §3ZA (unauthorised acts causing serious damage) |
| **Wireless Telegraphy Act 2006** | §8 (use of wireless telegraphy apparatus without a licence where required), §48 (deliberate interference with wireless telegraphy) |
| **Communications Act 2003** | §125–§127 (dishonest obtaining of electronic communications services, improper use of public electronic communications network) |
| **Data Protection Act 2018 / UK GDPR** | Interception of personal data in transit without lawful basis |

Penalties under these statutes range from unlimited fines to **up to 10 years' imprisonment** for the most serious offences.

> **The authors and contributors of this project accept no legal liability for any unlawful use of this firmware. You assume full and sole legal responsibility for every packet transmitted by a device you build and operate.**

---

## 4. Applicable Law, European Union

In EU member states, equivalent provisions exist under:

- **Directive 2013/40/EU** on attacks against information systems (transposed into national criminal codes)
- National implementations of the **European Electronic Communications Code (EECC, Directive 2018/1972)**

The principle is identical: transmitting deauthentication frames or capturing 802.11 handshakes on networks without authorisation is a criminal act in virtually all EU jurisdictions.

---

## 5. Applicable Law, United States

Relevant US statutes include:

- **Computer Fraud and Abuse Act (CFAA), 18 U.S.C. § 1030**, unauthorised access to protected computers
- **Electronic Communications Privacy Act (ECPA), 18 U.S.C. § 2511**, interception of electronic communications
- **FCC Part 15 / Part 97**, intentional interference with licensed radio communications

---

## 6. Your Responsibility Checklist

Before enabling `FEATURE_WIFI_AUDIT` and operating the device, confirm **all** of the following:

- [ ] I am the registered owner of the target network **or** I hold a signed, dated authorisation document from the network owner
- [ ] The test is taking place in a physically controlled environment where no third-party traffic will be disrupted
- [ ] I have notified all users of the network that testing will occur and they may experience disruption
- [ ] I am complying with all local, national, and supranational laws in the jurisdiction where I am operating
- [ ] I understand that deauthentication attacks disrupt service for every client on the targeted AP and that this disruption may affect people other than the intended target
- [ ] I am not operating this device in any public space, shared residential building, or workplace without explicit written scope

---

## 7. Deauthentication Frames, Specific Warning

The **802.11 deauthentication attack** (WiFi Food in this project) is a denial-of-service mechanism. Even when performed against a network you own:

- Neighbouring networks may be affected if channels overlap
- Emergency services or safety-critical equipment operating on WiFi may be disrupted
- In high-density environments (apartment blocks, offices, hospitals) this risk is heightened

**Do not use deauthentication features in any environment where disruption of third-party wireless traffic is a foreseeable risk.**

---

## 8. Handshake & PMKID Capture

Capturing WPA2 handshakes or PMKID frames is a **passive data collection activity**. Depending on jurisdiction, capturing data from networks you do not own may constitute an interception offence regardless of whether you subsequently decrypt the data. When in doubt, disable the audit module.

---

## 9. No Warranty

This software is provided **"AS IS"**, without warranty of any kind, express or implied, including but not limited to warranties of merchantability, fitness for a particular purpose, or non-infringement. In no event shall the authors be liable for any claim, damages, or other liability arising from the use or misuse of this software.

---

## 10. Responsible Disclosure

If you discover a vulnerability in this project's own code or firmware:

- Please open a **GitHub Security Advisory** (private disclosure) rather than a public issue
- See [SECURITY.md](SECURITY.md) for the full responsible disclosure policy

---

*By building, flashing, or operating firmware from this repository, you confirm that you have read and understood this disclaimer and that you accept sole legal responsibility for your use of this tool.*
