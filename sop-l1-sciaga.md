# SOP1 L1 – ściąga z funkcji

Wszystko, co omawia tutorial [L1 – System plików 1](https://sop.mini.pw.edu.pl/pl/sop1/lab/l1/). Na końcu jest dodatek z funkcji, których L1 nie omawia, a zadania przykładowe ich wymagają (`nftw`, `open`/`read`/`write`, `readv`), opisanych w [L2](https://sop.mini.pw.edu.pl/pl/sop1/lab/l2/).

**Jak czytać:** najpierw tabela (co robi, co zwraca, jak sprawdzić błąd). Szczegóły i pułapki są niżej, pod nazwą funkcji. Gotowe implementacje są w `sop-l1-lib.c` i `sop-l1-args.c`.

**Zasada ogólna:** każdą funkcję, która może zawieść, sprawdzaj przez `if (funkcja(...)) ERR("funkcja");`. Błąd zwykle kończy program.

---

## 1. Tabela – wszystko w jednym miejscu

### Katalogi

| Funkcja | Nagłówek | Co robi | Sukces / błąd | man |
|---|---|---|---|---|
| `opendir(path)` | `<dirent.h>` | otwiera katalog do czytania | `DIR*` / `NULL` | `man 3p fdopendir` |
| `readdir(dirp)` | `<dirent.h>` | zwraca kolejny wpis katalogu | `struct dirent*` / `NULL` = koniec **albo** błąd (patrz `errno`) | `man 3p readdir` |
| `closedir(dirp)` | `<dirent.h>` | zamyka katalog, zwalnia zasoby | `0` / `-1` | `man 3p closedir` |
| `struct dirent` | `<dirent.h>` | wpis: `d_name` (nazwa), `d_ino` (inode) | – | `man 0p dirent.h` |
| `mkdir(path, mode)` | `<sys/stat.h>` | tworzy pusty katalog z uprawnieniami | `0` / `-1` (`EEXIST` = już jest) | `man 3p mkdir` |
| `getcwd(buf, size)` | `<unistd.h>` | wpisuje do `buf` absolutną ścieżkę katalogu roboczego | `buf` / `NULL` | `man 3p getcwd` |
| `chdir(path)` | `<unistd.h>` | zmienia katalog roboczy procesu | `0` / `-1` | `man 3p chdir` |

### Informacje o pliku i uprawnienia

| Funkcja | Nagłówek | Co robi | Sukces / błąd | man |
|---|---|---|---|---|
| `stat(path, &st)` | `<sys/stat.h>` | info o pliku; **podąża za linkiem** (opisuje cel) | `0` / `-1` (`ENOENT` = brak) | `man 3p fstatat` |
| `lstat(path, &st)` | `<sys/stat.h>` | jak `stat`, ale opisuje **sam link** | `0` / `-1` | `man 3p fstatat` |
| `S_ISREG(st.st_mode)` | `<sys/stat.h>` | czy zwykły plik | 1 / 0 | `man 0p sys_stat.h` |
| `S_ISDIR(st.st_mode)` | `<sys/stat.h>` | czy katalog | 1 / 0 | |
| `S_ISLNK(st.st_mode)` | `<sys/stat.h>` | czy symlink (działa tylko po `lstat`) | 1 / 0 | |
| `chmod(path, mode)` | `<sys/stat.h>` | zmienia uprawnienia | `0` / `-1` | `man 3p chmod` |
| `access(path, amode)` | `<unistd.h>` | sprawdza istnienie/prawa: `F_OK`, `R_OK`, `W_OK`, `X_OK` | `0` = OK / `-1` | `man 3p access` |
| `umask(mask)` | `<sys/stat.h>` | ustawia maskę odejmowaną od uprawnień nowych plików | stara maska (nie ma błędu) | `man 3p umask` |
| `chown(...)` | `<unistd.h>` | zmiana właściciela (tylko root, nieomawiane) | | |

### Pliki – stdio (`FILE*`)

| Funkcja | Nagłówek | Co robi | Sukces / błąd | man |
|---|---|---|---|---|
| `fopen(path, mode)` | `<stdio.h>` | otwiera plik: `"r"`, `"w"`, `"w+"`, `"a"`, `"a+"`, `"r+"` | `FILE*` / `NULL` | `man 3p fopen` |
| `fclose(f)` | `<stdio.h>` | zamyka plik, zapisuje bufor | `0` / `EOF` | `man 3p fclose` |
| `fseek(f, offset, whence)` | `<stdio.h>` | ustawia kursor: `SEEK_SET` / `SEEK_CUR` / `SEEK_END` | `0` / `-1` | `man 3p fseek` |
| `ftell(f)` | `<stdio.h>` | aktualna pozycja kursora (po `fseek(f,0,SEEK_END)` = rozmiar) | pozycja / `-1` | `man 3p ftell` |
| `fread(ptr, size, n, f)` | `<stdio.h>` | czyta `n` elementów po `size` bajtów | liczba **całych** elementów (mniej = EOF/błąd) | `man 3p fread` |
| `fwrite(ptr, size, n, f)` | `<stdio.h>` | zapisuje `n` elementów | liczba zapisanych | `man 3p fwrite` |
| `fgets(buf, size, f)` | `<stdio.h>` | czyta linię (z `\n`) do bufora o rozmiarze `size` | `buf` / `NULL` = EOF/błąd | `man 3p fgets` |
| `fgetc(f)` / `fputc(c, f)` | `<stdio.h>` | jeden znak | znak / `EOF` | |
| `fputs(s, f)` | `<stdio.h>` | zapisuje string (bez dodawania `\n`) | ≥0 / `EOF` | |
| `fprintf(f, fmt, ...)` / `fscanf` | `<stdio.h>` | formatowany zapis/odczyt | liczba znaków/pól / <0 | |
| `getline(&line, &len, f)` | `<stdio.h>` | czyta całą linię, **sam alokuje** bufor | długość / `-1` | `man 3p getdelim` |
| `unlink(path)` | `<unistd.h>` | usuwa nazwę pliku | `0` / `-1` (`ENOENT` = nie było) | `man 3p unlink` |

### Dowiązania

| Funkcja | Nagłówek | Co robi | Sukces / błąd | man |
|---|---|---|---|---|
| `symlink(target, linkpath)` | `<unistd.h>` | tworzy symlink `linkpath -> target` | `0` / `-1` (`EEXIST`) | `man 3p symlink` |
| `readlink(path, buf, size)` | `<unistd.h>` | czyta, na co wskazuje link (**bez** `\0` na końcu!) | liczba bajtów / `-1` | `man 3p readlink` |
| `link(old, new)` | `<unistd.h>` | twardy link (na labach nie używamy) | `0` / `-1` | `man 3p link` |

### Błędy, liczby, losowość, wyjście

| Funkcja / makro | Nagłówek | Co robi | man |
|---|---|---|---|
| `errno` | `<errno.h>` | globalny kod ostatniego błędu | `man 3p errno` |
| `perror(msg)` | `<stdio.h>` | wypisuje `msg: opis błędu z errno` na stderr | `man 3p perror` |
| `ERR(source)` | własne makro | `perror` + plik:linia + `exit(EXIT_FAILURE)` | – |
| `exit(code)` | `<stdlib.h>` | kończy program (`EXIT_SUCCESS` / `EXIT_FAILURE`), opróżnia bufory | `man 3p exit` |
| `getopt(argc, argv, "p:n:s:")` | `<unistd.h>` | parsuje flagi; wartość w `optarg`, reszta od `argv[optind]` | `man 3p getopt` |
| `strtol(s, &end, base)` | `<stdlib.h>` | string → liczba w dowolnej bazie (8 dla uprawnień!) | `man 3p strtol` |
| `srand(seed)` / `rand()` | `<stdlib.h>` | losowość; `srand(time(NULL))` **raz** na proces | `man 3p rand` |
| `time(NULL)` | `<time.h>` | sekundy od epoki | `man 3p time` |
| `sleep(s)` | `<unistd.h>` | czeka `s` sekund | `man 3p sleep` |

### Komendy terminala do testowania

| Komenda | Po co |
|---|---|
| `ls -l` | uprawnienia, rozmiary, **na co wskazują linki** (`a -> b`) |
| `ln -s CEL LINK` | symlink (odpowiednik `symlink`) |
| `touch plik` | pusty plik |
| `echo tekst > plik`, `cat plik` | szybki zapis/podgląd |
| `mkdir -p a/b/c` | drzewo katalogów do testów |
| `chmod 000 katalog` | test „brak uprawnień" |
| `./prog > out.txt`, `./prog 2> err.txt` | przekierowanie stdout/stderr |
| `xxd plik` | podgląd pliku binarnego bajt po bajcie |

---

## 2. Szczegóły i pułapki

### `ERR` – obsługa błędów
```c
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
```
Wypisuje błąd z `errno` oraz plik i linię, potem kończy program. Pisze na stderr.
- Typowe użycie: `if (closedir(dirp)) ERR("closedir");` albo `if ((f = fopen(p, "r")) == NULL) ERR("fopen");`.
- Przypisanie w warunku daj **w nawiasy**: `if ((dp = readdir(dirp)) != NULL)`.

### `opendir` / `readdir` / `closedir` – przeglądanie katalogu
```c
DIR *opendir(const char *dirname);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);
```
Otwierasz, w pętli wołasz `readdir` aż do `NULL`, zamykasz.
- **`readdir` zwraca `NULL` i na końcu, i przy błędzie.** Ustaw `errno = 0` **bezpośrednio przed każdym** `readdir`, a po pętli: `if (errno != 0) ERR("readdir");`.
- **`d_name` to sama nazwa, nie ścieżka.** `stat(dp->d_name)` działa tylko wtedy, gdy katalog roboczy to ten katalog. Inaczej skorzystaj z `join_paths(dir, dp->d_name)`.
- W każdym katalogu są `.` i `..` (twarde linki do siebie i rodzica). Dlatego „pusty" katalog ma 2 katalogi. Często trzeba je pominąć: `strcmp(name, ".") == 0 || strcmp(name, "..") == 0`.
- Kolejność wpisów jest przypadkowa, nie alfabetyczna.
- Na bieżący katalog używaj `"."`, nie ma potrzeby liczyć jego ścieżki.
- Linux ma w `dirent` dodatkowe pola (np. `d_type`), ale **trzymaj się standardu**: typ sprawdzaj przez `lstat`.

```c
DIR* dirp = opendir(".");
if (!dirp) ERR("opendir");
struct dirent* dp;
while (1) {
    errno = 0;
    if ((dp = readdir(dirp)) == NULL) break;
    // dp->d_name
}
if (errno) ERR("readdir");
if (closedir(dirp)) ERR("closedir");
```

### `stat` / `lstat` – informacje o pliku
```c
int stat(const char *restrict path, struct stat *restrict buf);
int lstat(const char *restrict path, struct stat *restrict buf);
```
Wypełniają przekazaną strukturę `struct stat` (zadeklaruj ją jako zmienną i podaj `&st`).
- **Różnica tylko przy linkach:** `stat` opisuje plik docelowy, `lstat` sam link. Żeby policzyć linki, **musisz** użyć `lstat`.
- Najważniejsze pola: `st_size` (rozmiar w bajtach, typ `off_t`, wypisuj jako `printf("%lld", (long long)st.st_size)`), `st_mode` (typ + uprawnienia), `st_uid`, `st_mtime`, `st_ino`.
- Typ pliku: `S_ISREG(st.st_mode)`, `S_ISDIR(...)`, `S_ISLNK(...)`. Wszystko inne (FIFO, socket, urządzenie) to „inny/nieznany typ".
- Sprawdzenie, czy plik istnieje: `if (stat(path, &st)) { if (errno == ENOENT) /* nie ma */; else ERR("stat"); }`.
- **`restrict`** oznacza, że argumenty nie mogą wskazywać na tę samą pamięć. Podanie tego samego wskaźnika dwa razy to niezdefiniowane zachowanie.

### `getcwd` / `chdir` – katalog roboczy
```c
char *getcwd(char *buf, size_t size);
int chdir(const char *path);
```
`getcwd` zapisuje, gdzie jesteś, a `chdir` zmienia katalog (ścieżka względna albo absolutna).
- **Wzorzec:** zapamiętaj start przez `getcwd`, zrób `chdir(arg)`, wykonaj pracę i wróć przez `chdir(start)`. Bez powrotu kolejne **względne** argumenty liczyłyby się od złego miejsca (`dir1` i `dir2/dir3` dałoby `dir1/dir2/dir3`).
- Bufor musi być dość duży: `char path[PATH_MAX];` (`<limits.h>`) albo `MAX_PATH`.
- Nieudany `chdir` nie musi kończyć programu. Często wystarczy `if (chdir(argv[i])) { fprintf(stderr, ...); continue; }`.
- Katalog roboczy należy do procesu, więc przed wyjściem z programu nie trzeba go przywracać.
- **Nigdy `printf(argv[i])`**, tylko `printf("%s", argv[i])`. Nazwa z `%d` zostałaby potraktowana jak format.
- `chdir` w callbacku `nftw` psuje ścieżki, które podaje `nftw`. Do linków w innym katalogu lepszy jest `symlinkat` (patrz `sop-l1-lib.c`).

### `fopen` / `fclose` – otwieranie pliku
```c
FILE *fopen(const char *restrict pathname, const char *restrict mode);
int fclose(FILE *stream);
```

| tryb | czytanie | pisanie | jeśli nie istnieje | jeśli istnieje | kursor |
|---|---|---|---|---|---|
| `"r"` | tak | nie | błąd | – | początek |
| `"r+"` | tak | tak | błąd | zostaje | początek |
| `"w"` / `"w+"` | `w+` tak | tak | tworzy | **czyści do 0** | początek |
| `"a"` / `"a+"` | `a+` tak | tak | tworzy | zostaje | **zawsze koniec przy zapisie** |

- `"b"` (np. `"rb"`) na POSIX niczego nie zmienia, bo nie ma rozróżnienia tekst/binarny.
- `FILE` jest strukturą nieprzezroczystą. Trzymasz tylko wskaźnik, do pól nie zaglądasz.
- **Zawsze `fclose`.** Dopiero to gwarantuje, że buforowane dane trafiły na dysk.
- Uprawnienia nowego pliku z `fopen` to `0666 & ~umask`. **Nie da się nadać `x`.** Do pełnej kontroli służy `open(path, O_CREAT..., mode)`.
- `"w+"` na **istniejącym** pliku zachowuje jego stare uprawnienia. Żeby nadać nowe, najpierw `unlink`.

### `fseek` / `ftell` – kursor
```c
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
```
- `SEEK_SET`: od początku; `SEEK_CUR`: od bieżącej pozycji (ujemny offset cofa); `SEEK_END`: od końca.
- **Rozmiar pliku:** `fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);`. Prościej przez `stat(...).st_size`.
- W trybie `"a"` zapis i tak idzie na koniec, niezależnie od `fseek`.
- **Pisanie za końcem pliku** (po `fseek` dalej niż rozmiar) wypełnia dziurę zerami automatycznie. Tak `prog12` robi plik z zerami.
- `rewind(f)` to `fseek(f, 0, SEEK_SET)` plus wyczyszczenie flag błędu i EOF.

### `fread` / `fwrite` – dane binarne
```c
size_t fread(void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
size_t fwrite(const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
```
Czytają/zapisują `nitems` elementów po `size` bajtów i zwracają liczbę **całych** elementów.
- Wynik mniejszy niż `nitems` oznacza EOF albo błąd. Rozróżnisz przez `feof(f)` / `ferror(f)`.
- **Częściowy odczyt:** po `n` elementach wołaj dalej z `ptr + size*n` i `nitems - n`. Kursor przesuwa się sam.
- W praktyce `fread` jest używany częściej niż `fscanf`. Własna konwersja bajtów daje więcej kontroli.
- Liczby zapisane binarnie mają kolejność bajtów maszyny (x86 to little-endian).

### `fgets` / `getline` – czytanie linii
```c
char *fgets(char *restrict s, int n, FILE *restrict stream);
ssize_t getline(char **restrict lineptr, size_t *restrict n, FILE *restrict stream);
```
- **`fgets` NIC nie alokuje.** Bufor musi istnieć, np. `char buf[256]; fgets(buf, sizeof(buf), f);`.
  - `sizeof` daje rozmiar tylko dla **tablicy**. Dla `char*` to 8 bajtów (rozmiar wskaźnika)!
  - Zostawia `\n` na końcu. Usuniesz go przez `buf[strcspn(buf, "\n")] = 0;`.
  - Zbyt długa linia zostanie wczytana w kilku kawałkach.
- **`getline` alokuje sam.** Startujesz z `char* line = NULL; size_t len = 0;`, a na końcu robisz `free(line)`. Zwraca `-1` przy EOF.
  - `len` to rozmiar **zaalokowanego** bufora, nie długość linii. Długość to wartość zwrócona przez `getline` albo `strlen(line)`.
- Każde wywołanie czyta **następną** linię, bo pozycję pamięta `FILE*`. Gotowa funkcja: `read_line` w `sop-l1-lib.c`.

### Parsowanie stringów (przydatne w zadaniach)

| Funkcja | Co robi |
|---|---|
| `strchr(s, ':')` | wskaźnik na pierwszy `:` albo `NULL` |
| `strstr(s, "abc")` | wskaźnik na pierwsze wystąpienie podciągu albo `NULL` (to jest „grep") |
| `strcmp(a, b) == 0` | równe stringi |
| `strncmp(a, b, n) == 0` | równe pierwsze `n` znaków (prefiks) |
| `strcspn(s, "\n")` | indeks pierwszego `\n` (albo długość) – do ucinania |
| `strdup(s)` | kopia stringa na stercie → `free` |
| `strlen(s)` | długość w **bajtach** (polskie/greckie znaki to 2 bajty) |
| `snprintf(buf, size, fmt, ...)` | bezpieczne sklejanie stringów |

### `unlink` – usuwanie pliku
```c
int unlink(const char *path);
```
- Usuwa **nazwę**. Jeśli inny proces ma plik otwarty, dane żyją, dopóki ten proces go nie zamknie.
- Typowo przed tworzeniem pliku: `if (unlink(name) && errno != ENOENT) ERR("unlink");`, bo brak pliku to nie błąd.

### `mkdir` – tworzenie katalogu
```c
int mkdir(const char *path, mode_t mode);
```
- Typowy tryb to `0755`. Katalog **musi mieć `x`**, żeby dało się do niego wejść i dostać do zawartości.
- Tworzy tylko jeden poziom, rodzic musi istnieć.
- `EEXIST` oznacza, że katalog już jest. Zależnie od zadania to błąd albo coś do zignorowania: `if (mkdir(p, 0755) && errno != EEXIST) ERR("mkdir");`.

### `chmod` / `access` / `umask` – uprawnienia
```c
int chmod(const char *path, mode_t mode);
int access(const char *path, int amode);
mode_t umask(mode_t cmask);
```
- `chmod` wymaga prawa do zmiany pliku (bycia właścicielem).
- `access` przyjmuje **inne** flagi niż `chmod`/`mkdir`: `F_OK` (istnieje), `R_OK`, `W_OK`, `X_OK`, łączone przez `|`. Zwraca `0`, gdy wszystko OK.
- `umask` **odejmuje** bity od uprawnień nowych plików. Żeby dostać dokładnie `perms`, ustaw `umask(~perms & 0777)`. Zwraca starą maskę, nie ma błędu, działa tylko w tym procesie.
- Uprawnienia ósemkowo: `0644` = rw-r--r--, `0755` = rwxr-xr-x. Z argumentu wczytasz je przez `strtol(optarg, NULL, 8)`.

### `symlink` / `readlink` / `link` – dowiązania
```c
int symlink(const char *path1, const char *path2);   // tworzy path2 -> path1
ssize_t readlink(const char *restrict path, char *restrict buf, size_t bufsize);
```
- **Kolejność argumentów:** najpierw **CEL**, potem **gdzie położyć link** (jak `ln -s CEL LINK`).
- **Względny cel liczy się od katalogu, w którym leży LINK, a nie od katalogu roboczego.** Dla linku `b/file_bak` do pliku `a/file.txt`:
  - źle: `symlink("a/file.txt", "b/file_bak")`, bo link szuka `b/a/file.txt`;
  - dobrze: `symlink("../a/file.txt", "b/file_bak")`.

  Gotowa funkcja: `create_symlink_in_directory` w `sop-l1-lib.c`.
- Względny link przestaje działać, gdy przeniesiesz sam link. Absolutny przestaje działać, gdy przeniesiesz cel. Przenoszenie obu razem (cały katalog) zachowuje względne linki.
- `readlink` **nie dopisuje `\0`**: `ssize_t n = readlink(p, buf, sizeof(buf) - 1); if (n < 0) ERR("readlink"); buf[n] = 0;`.
- Twarde linki (`link`) to kolejna równorzędna nazwa tego samego inode'u. Na labach się ich nie używa (problemy z uprawnieniami i bezpieczeństwem).

### `getopt` / `strtol` – argumenty programu
```c
int getopt(int argc, char * const argv[], const char *optstring);
long strtol(const char *restrict str, char **restrict endptr, int base);
```
- `"p:n:s:"`: dwukropek po literze oznacza, że flaga ma wartość, dostępną w `optarg`. Po pętli argumenty bez flag zaczynają się od `argv[optind]`.
- `getopt` zwraca `'?'` dla nieznanej flagi i `-1` na końcu.
- `strtol(s, &end, 10)`: przy poprawnej liczbie `*end == 0` i `end != s`. `atoi` nie wykrywa błędów.
- Pełny szablon: `sop-l1-args.c`.

### `rand` / `srand` / `time`
- `srand(time(NULL))` wołasz **raz** na początku programu, a nie w pętli.
- `rand() % n` daje liczbę od 0 do n-1. `RAND_MAX` może być mały (np. 32767), więc dla dużych plików losowe pozycje nie pokryją całości.
- Losowa wielka litera: `'A' + rand() % ('Z' - 'A' + 1)`.

### Buforowanie stdout (eksperyment `prog13`)
- **`printf` jest buforowany.** Na terminalu wypisuje się dopiero po `\n`. Po przekierowaniu do pliku dopiero, gdy bufor się zapełni albo program skończy się normalnie.
- **Ctrl+C albo crash gubi zawartość bufora.** Normalne zakończenie (`return`/`exit`) zapisuje wszystko.
- **stderr nie jest buforowany**, więc `fprintf(stderr, ...)` pojawia się od razu. Używaj go do debugowania.
- Konwencja: stdout tylko na **wyniki**, a błędy i diagnostyka na stderr (tak robi `grep` i makro `ERR`).
- Wymuszenie wypisania: `fflush(stdout);`. Konieczne przy mieszaniu `printf` z `write(STDOUT_FILENO, ...)`.

### Typy z prototypów
Używaj typów z prototypów funkcji systemowych, nie `int`:
- `ssize_t`: rozmiar albo -1;
- `size_t`: rozmiar bez znaku;
- `off_t`: rozmiar pliku;
- `mode_t`: uprawnienia i typ.

### Najczęstsze wartości `errno`

| errno | Znaczenie | Typowa reakcja |
|---|---|---|
| `ENOENT` | plik/katalog nie istnieje | komunikat, czasem ignorujemy (`unlink`) |
| `EEXIST` | już istnieje (`mkdir`, `symlink`, `open` z `O_EXCL`) | błąd albo ignorujemy |
| `EACCES` | brak uprawnień | komunikat / `ERR` |
| `ENOTDIR` | część ścieżki nie jest katalogiem | jak `ENOENT` |
| `EINTR` | przerwane sygnałem | ponowić (`TEMP_FAILURE_RETRY`) |

---

## 3. Dodatek – omawiane w L2, a potrzebne w zadaniach przykładowych L1

| Funkcja | Nagłówek | Co robi | Sukces / błąd | man |
|---|---|---|---|---|
| `nftw(path, fn, fd_limit, flags)` | `<ftw.h>` | rekurencyjnie woła `fn` dla każdego wpisu w drzewie | `0` / `-1` / to, co zwrócił `fn` | `man 3p nftw` |
| `open(path, flags, mode)` | `<fcntl.h>` | otwiera plik → deskryptor (`int`) | `fd ≥ 0` / `-1` | `man 3p open` |
| `read(fd, buf, n)` | `<unistd.h>` | czyta **do** `n` bajtów | liczba bajtów / `0` = EOF / `-1` | `man 3p read` |
| `write(fd, buf, n)` | `<unistd.h>` | zapisuje **do** `n` bajtów | liczba bajtów / `-1` | `man 3p write` |
| `close(fd)` | `<unistd.h>` | zamyka deskryptor | `0` / `-1` | `man 3p close` |
| `readv(fd, iov, cnt)` / `writev` | `<sys/uio.h>` | jeden odczyt/zapis do/z wielu buforów | bajty / `-1` | `man 3p readv` |
| `TEMP_FAILURE_RETRY(expr)` | `<unistd.h>` + `_GNU_SOURCE` | powtarza `expr`, gdy błąd to `EINTR` | – | – |

### `nftw` – rekurencja po drzewie
```c
int nftw(const char *path, int (*fn)(const char *, const struct stat *, int, struct FTW *), int fd_limit, int flags);
int callback(const char *path, const struct stat *s, int type, struct FTW *f) { ...; return 0; }
```
- Wymaga `#define _XOPEN_SOURCE 500` (albo 700, albo `_GNU_SOURCE`) **przed** `#include`. Starego `ftw` nie używaj.
- `type`:
  - `FTW_F`: nie-katalog (zwykły plik **lub inny typ**, potwierdź przez `S_ISREG(s->st_mode)`);
  - `FTW_D`: katalog;
  - `FTW_SL`: symlink (tylko z `FTW_PHYS`);
  - `FTW_DNR`: katalog bez prawa odczytu;
  - `FTW_NS`: `stat` się nie udał.
- `f->level` to głębokość (start = 0). `path + f->base` to sama nazwa pliku bez katalogów.
- Flagi:
  - `FTW_PHYS`: nie wchodzi w linki, raportuje je jako `FTW_SL`. **Prawie zawsze tego chcesz.**
  - `FTW_DEPTH`: katalog po swojej zawartości.
  - `FTW_CHDIR`: `chdir` do każdego katalogu.
- `fd_limit` (np. 20) to maksymalna liczba otwartych deskryptorów.
- Callback zwraca `0`, żeby iść dalej. Wartość różna od zera **natychmiast przerywa** `nftw`.
- Callback nie dostaje własnych danych, więc **zmienne globalne są tu dozwolone** (liczniki, deskryptor katalogu).

### `open` / `read` / `write` / `close` – niskopoziomowo
- Flagi `open`, łączone przez `|`:
  - dostęp: `O_RDONLY`, `O_WRONLY`, `O_RDWR`;
  - `O_CREAT` (wymaga 3. argumentu `mode`, np. `0644`);
  - `O_TRUNC` (wyczyść plik);
  - `O_APPEND` (zawsze dopisuj na końcu);
  - `O_EXCL` (z `O_CREAT`: błąd, jeśli plik istnieje).
- **`read` i `write` mogą przenieść MNIEJ bajtów, niż prosisz.** Dlatego służą do tego `bulk_read` i `bulk_write`, które dociągają w pętli. `read` zwracające `0` oznacza EOF.
- Deskryptor to `int`, nie `FILE*`, więc `fprintf` na nim nie zadziała. Na stdout używasz `STDOUT_FILENO`.
- Bez `O_TRUNC` nadpisywany dłuższy plik zachowa „ogon" starej zawartości.
- Nie ma buforowania. Przy mieszaniu z `printf` najpierw zrób `fflush(stdout)`.

### `readv` / `writev` – wiele buforów naraz
```c
struct iovec { void *iov_base; size_t iov_len; };
ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
```
- Jedno wywołanie wypełnia kolejno `iov[0]`, `iov[1]` itd. Idealne do rekordów stałej długości (np. 4 bajty rozmiaru + 64 bajty tytułu).
- `iovcnt` ≤ `IOV_MAX` (zwykle 1024). Przy większej liczbie wektorów czytaj partiami.
- **`sizeof(struct)` ≠ suma pól** (padding). Wskazuj każde pole osobnym wektorem.
- Gotowce: `read_book_records_readv` i `write_book_records_writev` w `sop-l1-lib.c`.

---

## 4. Gdzie co jest gotowe

| Potrzebujesz | Funkcja w `sop-l1-lib.c` / `sop-l1-args.c` |
|---|---|
| wczytać linię (stdin/plik) | `read_line` |
| wartość pola `klucz:wartość` | `get_field_value` |
| grep po pliku | `grep_first_line`, `grep_all_lines` |
| typ pliku jako tekst | `file_type_name` |
| czy ścieżka istnieje | `path_exists` |
| rozmiar pliku | `get_file_size` |
| wypisać plik niskopoziomowo | `print_file_content_lowlevel` |
| lista plików w katalogu | `list_directory` |
| dopisywanie linii z stdin | `append_lines_from_stdin` |
| rekurencja | `walk_directory_recursive` + `print_entry_with_type` |
| katalog / względny symlink | `make_directory`, `create_symlink_in_directory` |
| binarna baza rekordów | `read_book_records_readv`, `write_book_records_writev` |
| menu CLI | `example_menu_interface` |
| flagi programu | `parse_program_options` (`sop-l1-args.c`) |
