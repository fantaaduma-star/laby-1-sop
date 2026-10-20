// =====================================================================================================
// SOP1 L1 – gotowe funkcje do labów (system plików). Kopiuj potrzebne funkcje + nagłówki + ERR.
// -----------------------------------------------------------------------------------------------------
// Kompilacja:   gcc -Wall -Wextra -fsanitize=address,undefined -o prog prog.c
// Sprawdzenie samej biblioteki (nie ma main):   gcc -Wall -Wextra -c sop-l1-lib.c
//
// WAŻNE przy kopiowaniu do szkieletu z zajęć:
//  * Szkielet często ma "#define _XOPEN_SOURCE 700" – wystarczy do getline/strdup/nftw/symlinkat,
//    ale NIE daje TEMP_FAILURE_RETRY – dlatego niżej jest zapasowa definicja (kopiuj ją razem z bulk_*).
//  * Każda funkcja, która zwraca char*, zwraca pamięć z malloc → po użyciu free().
//  * Wszystkie funkcje przy błędzie funkcji systemowej kończą program przez ERR (tego wymagają zadania).
//
// Spis treści:
//   1. Podstawa:          ERR, bulk_read, bulk_write, join_paths, read_line
//   2. Pliki tekstowe:    get_field_value, grep_first_line, grep_all_lines, truncate_string
//   3. stat / typy:       file_type_name, path_exists, get_file_size
//   4. Wyświetlanie:      print_file_content_lowlevel, list_directory
//   5. Zapis:             append_lines_from_stdin
//   6. Rekurencja (nftw): print_entry_with_type, walk_directory_recursive
//   7. Katalogi/linki:    make_directory, make_path_relative_to_directory, create_symlink_in_directory
//   8. Binarne rekordy:   BookRecord, read_book_records_readv, write_book_records_writev
//   9. Szkielety:         example_menu_interface, example_parse_arguments_getopt
// =====================================================================================================

#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

// Zapasowa definicja – w glibc jest tylko przy _GNU_SOURCE. Powtarza wywołanie przerwane sygnałem (EINTR).
#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression)                             \
    ({                                                             \
        long int _result;                                          \
        do                                                         \
            _result = (long int)(expression);                      \
        while (_result == -1L && errno == EINTR);                  \
        _result;                                                   \
    })
#endif

// Maksymalna liczba wektorów w jednym readv/writev (zwykle 1024).
#ifndef IOV_MAX
#define IOV_MAX 1024
#endif

// Ile deskryptorów nftw może mieć otwartych naraz (wystarczy 20 – głębokość drzewa nie jest ograniczona).
#define NFTW_MAX_OPEN_DESCRIPTORS 20

// =====================================================================================================
// 1. PODSTAWA
// =====================================================================================================

// bulk_read(fd, bufor, ile) -> czyta DOKŁADNIE "ile" bajtów (mniej tylko przy EOF); zwraca liczbę przeczytanych, 0 = EOF, -1 = błąd.
//   Zwykły read() może przeczytać mniej niż prosiłeś (sygnał, pipe) – ta funkcja dociąga w pętli.
//   Bufor NIE jest zakończony '\0' – jak chcesz printf("%s"), zrób bufor o 1 większy i wstaw 0 na [wynik].
ssize_t bulk_read(int file_descriptor, char* destination_buffer, size_t bytes_to_read)
{
    ssize_t bytes_read_now;
    ssize_t total_bytes_read = 0;
    do
    {
        bytes_read_now = TEMP_FAILURE_RETRY(read(file_descriptor, destination_buffer, bytes_to_read));
        if (bytes_read_now < 0)
            return bytes_read_now;
        if (bytes_read_now == 0)
            return total_bytes_read;  // EOF
        destination_buffer += bytes_read_now;
        total_bytes_read += bytes_read_now;
        bytes_to_read -= bytes_read_now;
    } while (bytes_to_read > 0);
    return total_bytes_read;
}

// bulk_write(fd, bufor, ile) -> zapisuje WSZYSTKIE "ile" bajtów; zwraca liczbę zapisanych albo -1 przy błędzie (SPRAWDZAJ wynik!).
//   Do stdout podaj STDOUT_FILENO – ale przedtem fflush(stdout), jeśli wcześniej był printf (inaczej kolejność się pomiesza).
ssize_t bulk_write(int file_descriptor, const char* source_buffer, size_t bytes_to_write)
{
    ssize_t bytes_written_now;
    ssize_t total_bytes_written = 0;
    do
    {
        bytes_written_now = TEMP_FAILURE_RETRY(write(file_descriptor, source_buffer, bytes_to_write));
        if (bytes_written_now < 0)
            return bytes_written_now;
        source_buffer += bytes_written_now;
        total_bytes_written += bytes_written_now;
        bytes_to_write -= bytes_written_now;
    } while (bytes_to_write > 0);
    return total_bytes_written;
}

// join_paths("a/b", "c") -> nowy string "a/b/c" (malloc, zrób free); sam dodaje '/' tylko gdy go brakuje.
char* join_paths(const char* first_path, const char* second_path)
{
    const size_t first_length = strlen(first_path);
    const int needs_slash = first_length > 0 && first_path[first_length - 1] != '/';
    char* joined_path = malloc(first_length + needs_slash + strlen(second_path) + 1);
    if (!joined_path)
        ERR("malloc");
    strcpy(joined_path, first_path);
    if (needs_slash)
        strcat(joined_path, "/");
    return strcat(joined_path, second_path);
}

// read_line(stdin albo FILE*) -> jedna linia BEZ '\n' (malloc, zrób free) albo NULL przy końcu pliku (EOF / Ctrl+D).
//   Długość linii nieograniczona (używa getline). Pusta linia (sam Enter) zwraca "" – NIE NULL – sprawdzaj line[0] == 0.
//   Usuwa też '\r' (pliki z Windowsa). Błąd odczytu kończy program.
char* read_line(FILE* input_stream)
{
    char* line_buffer = NULL;
    size_t allocated_buffer_size = 0;
    ssize_t line_length = getline(&line_buffer, &allocated_buffer_size, input_stream);
    if (line_length == -1)
    {
        free(line_buffer);
        if (ferror(input_stream))
            ERR("getline");
        return NULL;
    }
    if (line_length > 0 && line_buffer[line_length - 1] == '\n')
        line_buffer[--line_length] = 0;
    if (line_length > 0 && line_buffer[line_length - 1] == '\r')
        line_buffer[--line_length] = 0;
    return line_buffer;
}

// =====================================================================================================
// 2. PLIKI TEKSTOWE (format "klucz:wartość", szukanie jak grep)
// =====================================================================================================

// get_field_value(plik, "author") -> sama wartość z linii "author:Plutarch" czyli "Plutarch" (malloc, zrób free) albo NULL gdy brak klucza.
//   metadata_file – FILE* z fopen(path, "r"); funkcja sama robi rewind, więc wołaj ją kilka razy na tym samym pliku.
//   Klucz musi pasować DOKŁADNIE (cała część przed ':'), więc "title" nie złapie "title_gr" ani "uthor" nie złapie "author".
//   Linie bez ':' są pomijane. Przy powtórzonym kluczu wygrywa pierwszy. Wartość może być pusta ("" dla "genre:").
//   Przykład (etap 1 biblioteki):
//       char* author = get_field_value(f, "author");
//       printf("author: %s\n", author ? author : "missing!");
//       free(author);   // free(NULL) jest OK
char* get_field_value(FILE* metadata_file, const char* wanted_key)
{
    char* current_line;
    char* found_value = NULL;
    rewind(metadata_file);
    while (found_value == NULL && (current_line = read_line(metadata_file)) != NULL)
    {
        char* colon_position = strchr(current_line, ':');
        if (colon_position != NULL)
        {
            *colon_position = 0;  // current_line to teraz sam klucz, colon_position + 1 to wartość
            if (strcmp(current_line, wanted_key) == 0)
            {
                found_value = strdup(colon_position + 1);
                if (!found_value)
                    ERR("strdup");
            }
        }
        free(current_line);
    }
    return found_value;
}

// grep_first_line(plik, "wzorzec") -> pierwsza CAŁA linia zawierająca wzorzec w dowolnym miejscu, bez '\n' (malloc, zrób free) albo NULL.
//   Jak grep: "title" pasuje też do "title_gr:..." – chcesz dokładny klucz, użyj get_field_value albo wzorca "title:".
//   Robi rewind, więc można wołać wielokrotnie. Rozróżnia wielkość liter (strcasestr z _GNU_SOURCE, jeśli nie chcesz).
char* grep_first_line(FILE* searched_file, const char* pattern)
{
    char* current_line;
    rewind(searched_file);
    while ((current_line = read_line(searched_file)) != NULL)
    {
        if (strstr(current_line, pattern))
            return current_line;  // oddajemy bufor wołającemu – on robi free
        free(current_line);
    }
    return NULL;
}

// grep_all_lines(plik, "wzorzec", stdout) -> wypisuje do output_stream każdą linię zawierającą wzorzec; zwraca liczbę trafień (0 = nic).
//   Robi rewind. Nic nie trzeba zwalniać.
int grep_all_lines(FILE* searched_file, const char* pattern, FILE* output_stream)
{
    char* current_line;
    int matching_lines_count = 0;
    rewind(searched_file);
    while ((current_line = read_line(searched_file)) != NULL)
    {
        if (strstr(current_line, pattern))
        {
            fprintf(output_stream, "%s\n", current_line);
            matching_lines_count++;
        }
        free(current_line);
    }
    return matching_lines_count;
}

// truncate_string(tekst, 64) -> przycina string W MIEJSCU do max 64 bajtów (wstawia '\0'); tekst musi być modyfikowalny (np. z strdup).
//   Uwaga: liczy BAJTY, nie znaki – grecki/polski znak UTF-8 to 2 bajty i może zostać przecięty w połowie (zadania i tak mówią o bajtach).
void truncate_string(char* text_to_truncate, size_t max_bytes)
{
    if (strlen(text_to_truncate) > max_bytes)
        text_to_truncate[max_bytes] = 0;
}

// =====================================================================================================
// 3. STAT I TYPY PLIKÓW
// =====================================================================================================

// file_type_name(st.st_mode) -> "file" / "dir" / "symlink" / "unknown" (stały string, NIE zwalniaj).
//   Żeby kiedykolwiek dostać "symlink", stat musi być zrobiony przez lstat (stat podąża za linkiem i pokaże cel).
//   Zmień napisy pod treść zadania (np. "regular" / "directory" / "other").
const char* file_type_name(mode_t file_mode)
{
    if (S_ISREG(file_mode))
        return "file";
    if (S_ISDIR(file_mode))
        return "dir";
    if (S_ISLNK(file_mode))
        return "symlink";
    return "unknown";
}

// path_exists(path, &st, 0) -> 1 gdy ścieżka istnieje (i wypełnia st), 0 gdy nie istnieje (wypisuje "File does not exist!").
//   follow_symlinks: 1 = stat (link traktowany jak jego cel), 0 = lstat (widać sam link).
//   Inny błąd niż "nie istnieje" (np. brak uprawnień) kończy program przez ERR.
//   Typowo w menu:  if (!path_exists(path, &st, 1)) return 1;
int path_exists(const char* path, struct stat* out_file_stat, int follow_symlinks)
{
    int stat_result = follow_symlinks ? stat(path, out_file_stat) : lstat(path, out_file_stat);
    if (stat_result == 0)
        return 1;
    if (errno == ENOENT || errno == ENOTDIR)
    {
        printf("File does not exist!\n");
        return 0;
    }
    ERR("stat");
}

// get_file_size(path) -> rozmiar pliku w bajtach (off_t; wypisuj printf("%lld", (long long)size)).
//   Używa stat, więc dla linku zwraca rozmiar pliku docelowego. Brak pliku kończy program.
off_t get_file_size(const char* path)
{
    struct stat file_stat;
    if (stat(path, &file_stat))
        ERR("stat");
    return file_stat.st_size;
}

// =====================================================================================================
// 4. WYŚWIETLANIE ZAWARTOŚCI
// =====================================================================================================

// print_file_content_lowlevel(path) -> wypisuje cały plik na stdout używając TYLKO open/read/write/close (wymóg "funkcji niskopoziomowych").
//   Przed write robi fflush(stdout), żeby wcześniejsze printf nie pojawiły się PO zawartości pliku.
//   Nie dodaje '\n' na końcu – jak plik go nie ma, dopisz printf("\n") sam.
void print_file_content_lowlevel(const char* path)
{
    int file_descriptor = TEMP_FAILURE_RETRY(open(path, O_RDONLY));
    if (file_descriptor < 0)
        ERR("open");

    char chunk_buffer[4096];
    ssize_t bytes_in_chunk;
    fflush(stdout);
    while ((bytes_in_chunk = bulk_read(file_descriptor, chunk_buffer, sizeof(chunk_buffer))) > 0)
    {
        if (bulk_write(STDOUT_FILENO, chunk_buffer, bytes_in_chunk) < 0)
            ERR("write");
    }
    if (bytes_in_chunk < 0)
        ERR("read");

    if (TEMP_FAILURE_RETRY(close(file_descriptor)))
        ERR("close");
}

// list_directory(path, 1, 1) -> wypisuje nazwy plików w katalogu (bez rekurencji), opcjonalnie z typem; NIE zmienia katalogu roboczego.
//   skip_dot_entries: 1 = pomija "." i "..".   show_file_types: 1 = dopisuje " -- file/dir/symlink/unknown" (przez lstat).
//   Kolejność wpisów jest przypadkowa (taka jak zwraca readdir) – jak trzeba posortować, zbierz do tablicy i qsort.
//   errno = 0 przed readdir jest konieczne: NULL oznacza i koniec katalogu, i błąd – rozróżnia je tylko errno.
void list_directory(const char* directory_path, int skip_dot_entries, int show_file_types)
{
    DIR* directory_stream = opendir(directory_path);
    if (directory_stream == NULL)
        ERR("opendir");

    struct dirent* directory_entry;
    while (1)
    {
        errno = 0;
        directory_entry = readdir(directory_stream);
        if (directory_entry == NULL)
            break;

        const char* entry_name = directory_entry->d_name;
        if (skip_dot_entries && (strcmp(entry_name, ".") == 0 || strcmp(entry_name, "..") == 0))
            continue;

        if (show_file_types)
        {
            char* entry_full_path = join_paths(directory_path, entry_name);  // lstat potrzebuje ścieżki od cwd, nie samej nazwy
            struct stat entry_stat;
            if (lstat(entry_full_path, &entry_stat))
                ERR("lstat");
            printf("%s -- %s\n", entry_name, file_type_name(entry_stat.st_mode));
            free(entry_full_path);
        }
        else
            printf("%s\n", entry_name);
    }
    if (errno != 0)
        ERR("readdir");

    if (closedir(directory_stream))
        ERR("closedir");
}

// =====================================================================================================
// 5. ZAPIS DO PLIKÓW
// =====================================================================================================

// append_lines_from_stdin(path) -> czyta linie z stdin i dopisuje każdą (+ '\n') na KONIEC pliku przez open(O_APPEND)/write; kończy na pustej linii lub EOF.
//   Plik musi istnieć (nie ma O_CREAT – dodaj "| O_CREAT" i trzeci argument 0644, jeśli zadanie każe tworzyć).
//   Zadanie Dikaiopolis wymaga najpierw pokazać obecną zawartość: wywołaj wcześniej print_file_content_lowlevel(path).
void append_lines_from_stdin(const char* path)
{
    int file_descriptor = TEMP_FAILURE_RETRY(open(path, O_WRONLY | O_APPEND));
    if (file_descriptor < 0)
        ERR("open");

    char* typed_line;
    while ((typed_line = read_line(stdin)) != NULL)
    {
        if (typed_line[0] == 0)  // pusta linia = koniec
        {
            free(typed_line);
            break;
        }
        if (bulk_write(file_descriptor, typed_line, strlen(typed_line)) < 0 || bulk_write(file_descriptor, "\n", 1) < 0)
            ERR("write");
        free(typed_line);
    }

    if (TEMP_FAILURE_RETRY(close(file_descriptor)))
        ERR("close");
}

// =====================================================================================================
// 6. REKURENCYJNE PRZEJŚCIE (nftw)
// =====================================================================================================

// print_entry_with_type – GOTOWY CALLBACK do nftw: wypisuje "ścieżka -- typ" dla każdego pliku/katalogu; zwraca 0 = idź dalej.
//   Argumenty, które daje nftw:
//     entry_path      – ścieżka od miejsca startu, np. "library/1/432/the knights" (zaczyna się od tego, co podałeś do nftw)
//     entry_stat      – wynik stat/lstat dla wpisu (rozmiar: entry_stat->st_size)
//     entry_type_flag – FTW_F plik, FTW_D katalog, FTW_SL symlink (tylko z FTW_PHYS), FTW_DNR katalog bez praw odczytu, FTW_NS stat się nie udał
//     walk_info       – walk_info->level = głębokość (start = 0), entry_path + walk_info->base = SAMA nazwa pliku bez katalogów
//   Zwrócenie != 0 natychmiast przerywa całe nftw i nftw zwraca tę wartość.
//   Callback nie ma własnego argumentu "void* data" – dane przekazuj przez zmienną globalną (to dozwolone i normalne na labach).
int print_entry_with_type(const char* entry_path, const struct stat* entry_stat, int entry_type_flag, struct FTW* walk_info)
{
    (void)walk_info;
    const char* entry_type_text;
    switch (entry_type_flag)
    {
        case FTW_F:
            entry_type_text = "file";
            break;
        case FTW_D:
            entry_type_text = "dir";
            break;
        case FTW_SL:
            entry_type_text = "symlink";
            break;
        default:
            entry_type_text = file_type_name(entry_stat->st_mode);
            break;
    }
    printf("%s -- %s\n", entry_path, entry_type_text);
    return 0;
}

// walk_directory_recursive(path, callback) -> wywołuje callback dla KAŻDEGO wpisu w drzewie (łącznie z samym path) przez nftw z FTW_PHYS.
//   FTW_PHYS = nie wchodzi w symlinki (bez tego można się zapętlić). Dodaj "| FTW_DEPTH", żeby katalog był po swojej zawartości.
//   Zwraca to, co zwrócił callback, który przerwał (0 = przeszedł wszystko). Błąd nftw kończy program.
//   NIE rób chdir w callbacku – nftw podaje ścieżki względem katalogu, w którym był start.
int walk_directory_recursive(const char* start_path, int (*entry_callback)(const char*, const struct stat*, int, struct FTW*))
{
    int walk_result = nftw(start_path, entry_callback, NFTW_MAX_OPEN_DESCRIPTORS, FTW_PHYS);
    if (walk_result == -1)
        ERR("nftw");
    return walk_result;
}

// =====================================================================================================
// 7. KATALOGI I DOWIĄZANIA SYMBOLICZNE
// =====================================================================================================

// make_directory(path, 0) -> tworzy JEDEN katalog (0755); fail_if_exists: 1 = istniejący to błąd (ERR), 0 = istniejący jest OK.
//   Nie tworzy rodziców – "index/by-genre/comedy" wymaga istniejącego "index/by-genre" (twórz po kolei).
//   Przykład z biblioteki: make_directory("index", 1) ("jeśli istnieje zwróć błąd"), make_directory(genre_dir, 0).
void make_directory(const char* directory_path, int fail_if_exists)
{
    if (mkdir(directory_path, 0755) == 0)
        return;
    if (errno == EEXIST && !fail_if_exists)
        return;
    ERR("mkdir");
}

// make_path_relative_to_directory("index/by-title", "library/1/frogs") -> "../../library/1/frogs" (malloc, zrób free).
//   Czyli: jak dostać się do pliku (ścieżka od cwd) z wnętrza katalogu link_directory (też od cwd). Tego potrzebuje względny symlink.
//   Obie ścieżki muszą być względne od cwd i BEZ "." / ".." / "./" w środku. Absolutna ścieżka pliku jest zwracana bez zmian.
char* make_path_relative_to_directory(const char* link_directory, const char* target_path_from_cwd)
{
    if (target_path_from_cwd[0] == '/')
    {
        char* absolute_copy = strdup(target_path_from_cwd);
        if (!absolute_copy)
            ERR("strdup");
        return absolute_copy;
    }

    size_t directory_depth = 0;
    int inside_path_component = 0;
    for (const char* character = link_directory; *character; character++)
    {
        if (*character == '/')
            inside_path_component = 0;
        else if (!inside_path_component)
        {
            inside_path_component = 1;
            directory_depth++;
        }
    }

    char* relative_path = malloc(directory_depth * 3 + strlen(target_path_from_cwd) + 1);
    if (!relative_path)
        ERR("malloc");
    relative_path[0] = 0;
    for (size_t level = 0; level < directory_depth; level++)
        strcat(relative_path, "../");
    return strcat(relative_path, target_path_from_cwd);
}

// create_symlink_in_directory("index/by-title", "the frogs", "library/1/the frogs", 0) -> tworzy WZGLĘDNY link index/by-title/the frogs → ../../library/1/the frogs.
//   link_directory – katalog na link (od cwd); link_name – nazwa linku (NIE może zawierać '/'); target_path_from_cwd – plik docelowy od cwd.
//   Zwraca 0 gdy utworzono, -1 gdy link już istniał i ignore_if_exists = 1; gdy istniał a ignore_if_exists = 0 → ERR (np. "powtórzona nazwa = błąd").
//   Używa symlinkat (bez chdir), więc bezpiecznie działa wewnątrz callbacku nftw. Sprawdź link: ls -l index/by-title
int create_symlink_in_directory(const char* link_directory, const char* link_name, const char* target_path_from_cwd, int ignore_if_exists)
{
    char* relative_target_path = make_path_relative_to_directory(link_directory, target_path_from_cwd);

    int directory_descriptor = TEMP_FAILURE_RETRY(open(link_directory, O_RDONLY | O_DIRECTORY));
    if (directory_descriptor < 0)
        ERR("open");

    int creation_result = 0;
    if (symlinkat(relative_target_path, directory_descriptor, link_name) != 0)
    {
        if (errno == EEXIST && ignore_if_exists)
            creation_result = -1;
        else
            ERR("symlinkat");
    }

    if (TEMP_FAILURE_RETRY(close(directory_descriptor)))
        ERR("close");
    free(relative_target_path);
    return creation_result;
}

// =====================================================================================================
// 8. BINARNE REKORDY (readv / writev) – format bazy z biblioteki: [4 bajty unsigned int rozmiar][64 bajty tytułu, dopełnione zerami]
// =====================================================================================================

#define BOOK_TITLE_BYTES 64
#define BOOK_RECORD_BYTES (sizeof(unsigned int) + BOOK_TITLE_BYTES)  // 68 bajtów na dysku

// Rekord w pamięci: title ma 65 bajtów, ostatni zawsze 0 → string poprawnie zakończony nawet przy pełnych 64 bajtach.
// UWAGA: sizeof(BookRecord) to NIE 68 (kompilator dodaje padding) – dlatego czytamy pola osobnymi wektorami, a nie całą strukturę.
typedef struct
{
    unsigned int file_size;
    char title[BOOK_TITLE_BYTES + 1];
} BookRecord;

// read_book_records_readv(path, &count) -> tablica rekordów z pliku bazy wczytana przez readv (malloc, zrób free); count = st_size / 68.
//   Każdy rekord to 2 wektory iovec (4 bajty rozmiaru + 64 bajty tytułu) – zmień pola/rozmiary pod inny format.
//   readv przyjmuje max IOV_MAX (~1024) wektorów naraz, więc czytamy partiami – inaczej przy >512 rekordach readv zwróci EINVAL.
//   Plik, którego rozmiar nie dzieli się przez 68, daje ostrzeżenie (resztka jest ignorowana). Liczby są w kolejności bajtów maszyny (little-endian).
BookRecord* read_book_records_readv(const char* database_path, int* out_record_count)
{
    int database_descriptor = TEMP_FAILURE_RETRY(open(database_path, O_RDONLY));
    if (database_descriptor < 0)
        ERR("open");

    struct stat database_stat;
    if (fstat(database_descriptor, &database_stat))
        ERR("fstat");
    if (!S_ISREG(database_stat.st_mode))
    {
        fprintf(stderr, "%s is not a regular file\n", database_path);
        exit(EXIT_FAILURE);
    }
    if (database_stat.st_size % BOOK_RECORD_BYTES != 0)
        fprintf(stderr, "warning: %s size is not a multiple of %zu\n", database_path, BOOK_RECORD_BYTES);

    const int record_count = database_stat.st_size / BOOK_RECORD_BYTES;
    BookRecord* records = calloc(record_count + 1, sizeof(BookRecord));  // calloc zeruje → title[64] == 0; +1 żeby nie było calloc(0)
    struct iovec* io_vectors = calloc(2 * record_count + 1, sizeof(struct iovec));
    if (!records || !io_vectors)
        ERR("calloc");

    for (int record_index = 0; record_index < record_count; record_index++)
    {
        io_vectors[2 * record_index].iov_base = &records[record_index].file_size;
        io_vectors[2 * record_index].iov_len = sizeof(unsigned int);
        io_vectors[2 * record_index + 1].iov_base = records[record_index].title;
        io_vectors[2 * record_index + 1].iov_len = BOOK_TITLE_BYTES;
    }

    const int total_vectors = 2 * record_count;
    const int vectors_per_call = IOV_MAX - IOV_MAX % 2;  // parzyście, żeby rekord nie był rozcięty między wywołaniami
    for (int first_vector = 0; first_vector < total_vectors; first_vector += vectors_per_call)
    {
        int vectors_in_this_call = total_vectors - first_vector;
        if (vectors_in_this_call > vectors_per_call)
            vectors_in_this_call = vectors_per_call;
        ssize_t expected_bytes = (ssize_t)(vectors_in_this_call / 2) * BOOK_RECORD_BYTES;
        ssize_t bytes_read = TEMP_FAILURE_RETRY(readv(database_descriptor, io_vectors + first_vector, vectors_in_this_call));
        if (bytes_read < 0)
            ERR("readv");
        if (bytes_read != expected_bytes)
        {
            fprintf(stderr, "readv: short read from %s\n", database_path);
            exit(EXIT_FAILURE);
        }
    }

    free(io_vectors);
    if (TEMP_FAILURE_RETRY(close(database_descriptor)))
        ERR("close");
    *out_record_count = record_count;
    return records;
}

// write_book_records_writev(path, records, count) -> zapisuje rekordy w tym samym 68-bajtowym formacie przez writev (tworzy/nadpisuje plik 0644).
//   Przydaje się do zrobienia własnej bazy testowej albo gdy zadanie każe bazę wygenerować. Tytuły dłuższe niż 64 bajty są ucinane.
void write_book_records_writev(const char* database_path, BookRecord* records, int record_count)
{
    int database_descriptor = TEMP_FAILURE_RETRY(open(database_path, O_WRONLY | O_CREAT | O_TRUNC, 0644));
    if (database_descriptor < 0)
        ERR("open");

    struct iovec* io_vectors = calloc(2 * record_count + 1, sizeof(struct iovec));
    if (!io_vectors)
        ERR("calloc");
    for (int record_index = 0; record_index < record_count; record_index++)
    {
        // strncpy dopełnia zerami do 64 bajtów – dokładnie tak wygląda format na dysku
        char title_padded_with_zeros[BOOK_TITLE_BYTES];
        strncpy(title_padded_with_zeros, records[record_index].title, BOOK_TITLE_BYTES);
        memcpy(records[record_index].title, title_padded_with_zeros, BOOK_TITLE_BYTES);
        records[record_index].title[BOOK_TITLE_BYTES] = 0;

        io_vectors[2 * record_index].iov_base = &records[record_index].file_size;
        io_vectors[2 * record_index].iov_len = sizeof(unsigned int);
        io_vectors[2 * record_index + 1].iov_base = records[record_index].title;
        io_vectors[2 * record_index + 1].iov_len = BOOK_TITLE_BYTES;
    }

    const int total_vectors = 2 * record_count;
    const int vectors_per_call = IOV_MAX - IOV_MAX % 2;
    for (int first_vector = 0; first_vector < total_vectors; first_vector += vectors_per_call)
    {
        int vectors_in_this_call = total_vectors - first_vector;
        if (vectors_in_this_call > vectors_per_call)
            vectors_in_this_call = vectors_per_call;
        ssize_t expected_bytes = (ssize_t)(vectors_in_this_call / 2) * BOOK_RECORD_BYTES;
        if (TEMP_FAILURE_RETRY(writev(database_descriptor, io_vectors + first_vector, vectors_in_this_call)) != expected_bytes)
            ERR("writev");
    }

    free(io_vectors);
    if (TEMP_FAILURE_RETRY(close(database_descriptor)))
        ERR("close");
}

// =====================================================================================================
// 9. SZKIELETY DO PRZEROBIENIA
// =====================================================================================================

// example_menu_interface() -> menu "1. show 2. write 3. walk 4. exit" jak w zadaniu Dikaiopolis; zwraca 1 = pokaż menu jeszcze raz, 0 = wyjście.
//   Użycie w main:  while (example_menu_interface()) ;
//   Schemat: wypisz opcje → wczytaj numer → zły = komunikat i return 1 → wczytaj ścieżkę → nie istnieje = return 1 → wywołaj etap → return 1.
//   Podmień wywołania w switch na swoje *_stageX(path, &file_stat).
int example_menu_interface(void)
{
    printf("1. show\n2. write\n3. walk\n4. exit\n");

    char* chosen_option = read_line(stdin);
    if (chosen_option == NULL)  // Ctrl+D – traktujemy jak exit
        return 0;
    int is_valid_option = strlen(chosen_option) == 1 && chosen_option[0] >= '1' && chosen_option[0] <= '4';
    char option_number = chosen_option[0];
    free(chosen_option);
    if (!is_valid_option)
    {
        printf("Incorrect operation.\n");
        return 1;
    }
    if (option_number == '4')
        return 0;

    char* typed_path = read_line(stdin);
    if (typed_path == NULL)
        return 0;
    struct stat file_stat;
    if (!path_exists(typed_path, &file_stat, 1))
    {
        free(typed_path);
        return 1;
    }

    switch (option_number)
    {
        case '1':  // show: plik → rozmiar + zawartość, katalog → lista nazw, inne → nieznany typ
            if (S_ISREG(file_stat.st_mode))
            {
                printf("Size: %lld bytes\n", (long long)file_stat.st_size);
                print_file_content_lowlevel(typed_path);
                printf("\n");
            }
            else if (S_ISDIR(file_stat.st_mode))
                list_directory(typed_path, 0, 0);
            else
                printf("Unknown file type.\n");
            break;
        case '2':  // write: najpierw pokaż obecną zawartość, potem dopisuj linie aż do pustej
            print_file_content_lowlevel(typed_path);
            printf("\n");
            append_lines_from_stdin(typed_path);
            break;
        case '3':  // walk: rekurencyjnie z typem każdego pliku
            walk_directory_recursive(typed_path, print_entry_with_type);
            break;
    }
    free(typed_path);
    return 1;
}

// example_parse_arguments_getopt(argc, argv) -> szablon getopt dla "prog -p ścieżka [-p ścieżka...] [-n liczba] [-v] [argumenty...]".
//   ":" po literze w "p:n:v" = opcja wymaga wartości (jest w optarg). Po pętli argv[optind..argc-1] to argumenty bez flag.
//   getopt sam wypisze błąd przy nieznanej opcji i zwróci '?' – wtedy wołamy usage i kończymy.
void example_parse_arguments_getopt(int argc, char** argv)
{
    int option_character;
    int number_value = 1;
    int verbose_enabled = 0;
    while ((option_character = getopt(argc, argv, "p:n:v")) != -1)
    {
        switch (option_character)
        {
            case 'p':
                printf("path: %s\n", optarg);  // -p można podać wiele razy – każde wywołanie trafia tutaj
                break;
            case 'n':
                number_value = atoi(optarg);  // atoi zwraca 0 dla śmieci – jak trzeba walidować, użyj strtol
                if (number_value <= 0)
                {
                    fprintf(stderr, "USAGE: %s -p path [-n positive_number] [-v]\n", argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'v':
                verbose_enabled = 1;
                break;
            default:
                fprintf(stderr, "USAGE: %s -p path [-n positive_number] [-v]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    printf("n = %d, verbose = %d\n", number_value, verbose_enabled);
    for (int argument_index = optind; argument_index < argc; argument_index++)
        printf("extra argument: %s\n", argv[argument_index]);
}
