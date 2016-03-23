
Per compilare LibTiff, bisogna ricordarsi di:


1. Compilare PRIMA zlib e LibJPEG

2. (Windows) Creare i file 'tif_config.h' e 'tiffconf.h' copiandoli dalle
   rispettive versioni platform-dependent

3. (Windows) Dentro tiffconf.h, abilitare il supporto alle compressioni JPEG e ZLIB

4. (Windows) NON usare 'tif_win32.c'. Anche li' usa 'tif_unix.c'.

5. Ricordarsi di inserire la funzione di API TIFFFdOpenNoCloseProc(..)
   che serve a deallocare la roba allocata da LibTIFF senza chiudere
   il file aperto

6. Per essere sicuri che sia andato tutto bene, provare a visualizzare
   il folder tif dentro la nostra libreria di immagini test (verify_image)


Daniele