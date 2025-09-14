# stm32l1_bootloader
## simple bootloader for stm32l1
<img width="1891" height="1017" alt="image" src="https://github.com/user-attachments/assets/0bf83a61-eb07-4faf-b30a-83d18ac1fa5b" />

https://github.com/user-attachments/assets/61cf27cb-da14-4e01-8a5e-09f52eefea33

## how to work

### Linker’da bellek yerleşimi (BOOT) ###
```
FLASH (32 KB)
0x0800_0000                  +-----------------------------+  BOOT vektör tablosu (.isr_vector)
                             |    BOOT .text / .rodata    |  (BOOT kod/sabitler)
MY_MEMORY (32KB)             +-----------------------------+
                             |    (BOOT kalan FLASH)      |
                             +-----------------------------+  ← API_SHARED 0x8018000
                             |   APP alanı (ayrı proje)   |  (APP vektör tablosu + APP kodu)
                             +-----------------------------+
                
                RAM (32 KB)
                0x2000_8000  +-----------------------------+  _estack  (MSP başlangıcı)
                             |           STACK             |  (aşağı büyür)
                             +-----------------------------+
                             |       serbest/tampon        |  (özel section ayırılabilir)
                             +-----------------------------+
                             |            HEAP             |  (yukarı büyür)
                             +-----------------------------+
                             | .bss / .data (BOOT)         |
                0x2000_0000  +-----------------------------+
```
* Linker script: FLASH/RAM sınırları, vektör tablosu konumu ve minimum stack/heap’i tanımlar.
* APP için kullanılacak FLASH bölgesi, BOOT alanından sonra başlar (sembolik: FLASH_APP_ADDR).

### go2APP() fonksiyonu ###
```
[BOOT çalışıyor] → go2APP()
   1) FLASH_APP_ADDR’deki ilk word = APP’in init MSP değeri
      → RAM aralığına düşüyor mu (maske kontrolü)?
   2) Evetse:
      - jump_address = *(FLASH_APP_ADDR + 4)   // Reset_Handler adresi
      - __set_MSP(*(FLASH_APP_ADDR))           // APP’in MSP’sini yükle
      - jump_to_app()                          // Reset_Handler’a zıpla
   3) Hayırsa:
      - "APP NOT FOUND" → BOOT modunda kal
```
```
        +-------------------------+
Reset → |   BOOT vektör tablosu   |
        +-----------+-------------+
                    |
                    v
              BOOT main()
                    |
                    v
                 go2APP()
       +-----------+-----------+
       |  MSP kontrol OK ?     |
       +-----------+-----------+
                   | Evet
                   v
   Set MSP → Read Reset_Handler → Jump (APP)
                   |
                  Hayır
                   v
           BOOT modunda devam et

```
* Bu akışta BOOT, APP’in vektör tablosunu kullanarak “gerçek başlangıç” olan Reset_Handler’a dallanır. (APP kendi .data/.bss hazırlığını yapar ve main()’ine geçer.)

### Linker’da sabit adresli API tablosu bölgesi ###
* BOOT linker script’inde, BOOT’un yayınlayacağı API tablosu için sabit bir FLASH adresi belirlenir:
```
/* Örn: 0x0801_8000 adresi API tablosunun başlangıcı */
MEMORY { /* ... */ MY_MEMORY (rx) : ORIGIN = 0x08018000, LENGTH = 32K }

.API_SHARED 0x08018000 :
{
  KEEP(*(.API_SHARED))
} > MY_MEMORY

```
```
FLASH (BOOT)
0x0801_8000  +-----------------------------+
             |  .API_SHARED (sabit adres)  | → [fonksiyon adresleri tablosu]
             +-----------------------------+
             |  (MY_MEMORY’nin kalanı)     |
             +-----------------------------+

```
### API header ###
* Her iki proje de aynı struct’ı bilir:
```
struct BootloaderSharedAPI {
  void (*Blink)(uint32_t dlyTicks);
  void (*TurnOn)(void);
  void (*TurnOff)(void);
};

```
* Bu yapı: fonksiyon işaretçileri tablosunun tipi (sıra ve imzalar = sözleşme).

### Tabloyu doldurma ###
* BOOT, fonksiyon adreslerini bu tabloya link zamanında yazar ve tabloyu sabit adrese koyar:
```
__attribute__((section(".API_SHARED"), used, aligned(4)))
const struct BootloaderSharedAPI g_BootApi = {
  .Blink  = Blink,
  .TurnOn = TurnOn,
  .TurnOff= TurnOff
};
```
```
0x0801_8000  +-------------------------------+
             | g_BootApi (const tablo)       |
             |  ├─ &Blink    (adres)         |
             |  ├─ &TurnOn    (adres)        |
             |  └─ &TurnOff   (adres)        |
             +-------------------------------+

```
* BOOT çalışırken ekstra bir “kurulum” yapmaz; tablo zaten FLASH’ta hazırdır.


### Tabloyu kullanma (APP tarafı) ###
* APP, sabit adrese pointer cast yapar ve fonksiyonları çağırır:
```
const struct BootloaderSharedAPI *api =
   (const struct BootloaderSharedAPI*)0x08018000;  // sabit tablo adresi

api->Blink(200);
api->TurnOn();
api->TurnOff();

```
```
APP main()
   |
   v
api = (BootloaderSharedAPI*) 0x0801_8000  (sabit adres)
   |
   v
api->Blink(...)  →  tabloda kayıtlı adrese dal → BOOT’taki Blink()

```
### GENEL ŞEMA ###

```
[FLASH]                                [RAM]
+----------------------+               +----------------------+
| BOOT vektör tablosu  |               |  .data/.bss (BOOT)   |
| BOOT .text/.rodata   |               |  Heap / Stack        |
|                      |               +----------------------+
| 0x08018000           |
| +------------------+ |   APP: api = (BootloaderSharedAPI*)0x08018000
| | .API_SHARED      | |   api->Blink() → &Blink (BOOT kodu)
| |  &Blink          | |
| |  &TurnOn         | |
| |  &TurnOff        | |
| +------------------+ |
|                      |
| ...                  |
+----------+-----------+
           |
           v  (FLASH_APP_ADDR)
+----------------------+   APP vektör tablosu + APP kodu
|        APP            |
+----------------------+

```


## tutorial
<img width="703" height="588" alt="image" src="https://github.com/user-attachments/assets/e9b2814e-8c2b-4325-9943-d5d9d1f423a0" />
<img width="1910" height="1021" alt="image" src="https://github.com/user-attachments/assets/585e0879-c908-4609-af4f-6deb9c932727" />
<img width="1917" height="1027" alt="image" src="https://github.com/user-attachments/assets/55649b92-c178-4270-b653-df49fb7ca561" />
<img width="1251" height="783" alt="image" src="https://github.com/user-attachments/assets/ddd147a6-7854-4069-b219-d72a5b0cd14c" />
<img width="695" height="480" alt="image" src="https://github.com/user-attachments/assets/499e108d-a3aa-4864-b3b9-0a5f7dd1af74" />
<img width="678" height="413" alt="image" src="https://github.com/user-attachments/assets/7144542e-cd47-4a69-858f-1e34f1a76ac4" />

