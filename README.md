# Lumen – Optical Tube Compressor

Compressore ottico a valvole ispirato al comportamento dei classici compressori ottici da studio. VST3 e Audio Unit per macOS (Apple Silicon e Intel).

## Controlli

| Controllo | Range | Note |
|---|---|---|
| Threshold | -40 … +20 dB | 0 dB = -18 dBFS. È il punto in cui la riduzione arriva a 1 dB (ginocchio morbido) |
| Ratio | 2:1 … 10:1 | continuo |
| Gain | 0 … +30 dB | guadagno di uscita, dopo la compressione |
| Timing | Fixed / Manual / Fix-Man | Fixed: attacco 1 ms, rilascio 50 ms. Manual: usa Attack e Release. Fix/Man: attacco veloce, i picchi brevi rilasciano subito, i segnali più lunghi del tempo di Attack passano al rilascio manuale |
| Attack | 0.5 … 300 ms | |
| Release | 0.05 … 10 s | |
| Tube | 0 … 100% | saturazione valvolare (2a armonica) e colore del trasformatore, in oversampling 2x |
| Mix | 0 … 100% | compressione parallela, con ritardo compensato |
| SC Low Cut | Off … 300 Hz | filtro passa-alto solo sul segnale che pilota la compressione |
| Stereo Link | on/off | i due canali comprimono insieme |
| Ext Sidechain | on/off | usa l'ingresso sidechain della traccia |

Il rilascio ha un carattere "ottico": più rapido dalle riduzioni profonde, coda più lenta vicino a 0 dB, e più lento dopo compressioni forti e prolungate.

## Sotto il cofano

- Tutti i tempi e i filtri sono calcolati dalla frequenza di campionamento: stesso suono da 44.1 a 192 kHz (verificato dai test).
- Parametri smussati: niente zipper noise durante l'automazione.
- Oversampling sullo stadio valvolare contro l'aliasing, latenza dichiarata alla DAW.
- Protezione da NaN e denormali; uscita limitata a +12 dBFS in caso di valori anomali.
- Supporta tracce mono e stereo, con sidechain esterno opzionale.

## Compilazione

Ogni modifica caricata su GitHub avvia la compilazione automatica (tab **Actions**): test del DSP, validazione `auval` e `pluginval`, poi il pacchetto **Lumen-macOS** pronto da scaricare.

In locale: `cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" && cmake --build build --config Release`

Basato su [JUCE](https://juce.com) (licenza gratuita per uso personale).
