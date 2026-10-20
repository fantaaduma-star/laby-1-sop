// =====================================================================================================
// SOP1 L1 – czytanie flag programu (-p ścieżka, -n 5, -v). DWIE funkcje, każdą wołasz osobno dla każdej flagi.
// -----------------------------------------------------------------------------------------------------
// CO SKOPIOWAĆ:  nagłówki + usage + read_repeated_flag i/lub read_single_flag. main na dole to tylko przykład.
//
// Oba wywołania dostają ten sam string ALL_FLAGS = wszystkie flagi programu, np. "p:n:v":
//     litera z ':' po niej  = flaga z wartością   (-p ścieżka, -n 5)
//     litera bez ':'        = flaga bez wartości  (-v)
// Nieznana flaga albo brak wartości → usage (komunikat + exit).
//
// Kompilacja i test:  gcc -Wall -Wextra -o args sop-l1-args.c && ./args -p a -p b -n 5 -v
// =====================================================================================================

#define _GNU_SOURCE

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// usage(argv[0]) -> wypisuje poprawne użycie i KOŃCZY program. Zmień tekst pod zadanie.
void usage(const char* program_name)
{
    fprintf(stderr, "USAGE: %s -p path [-p path ...] -n number [-v]\n", program_name);
    exit(EXIT_FAILURE);
}

// read_repeated_flag(argc, argv, "p:n:v", 'p', paths) -> wpisuje do paths wartości WSZYSTKICH wystąpień -p, zwraca ich liczbę (0 = nie było).
//   found_values – tablica, którą tworzysz sam:  char* paths[argc];   (więcej wartości niż argc być nie może)
//   Wartości to wskaźniki do argv – nie kopiuj, nie rób free.
//   Przykład:  ./prog -p a -n 5 -p b   →   paths = {"a", "b"}, zwraca 2
int read_repeated_flag(int argc, char** argv, const char* all_flags, char wanted_flag, char** found_values)
{
    int found_count = 0;
    int option_character;
    optind = 1;  // zaczynamy od początku argv – dzięki temu funkcję można wołać wiele razy
    while ((option_character = getopt(argc, argv, all_flags)) != -1)
    {
        if (option_character == '?')  // nieznana flaga albo brak wartości (getopt już wypisał komunikat)
            usage(argv[0]);
        if (option_character == wanted_flag)
            found_values[found_count++] = optarg;
    }
    return found_count;
}

// read_single_flag(argc, argv, "p:n:v", 'n') -> wartość flagi -n albo NULL, gdy jej nie podano; drugie wystąpienie → usage.
//   Dla flagi BEZ wartości (np. 'v') zwraca "" gdy jest, NULL gdy jej nie ma  →  int verbose = read_single_flag(...) != NULL;
//   Flaga wymagana?  if (value == NULL) usage(argv[0]);
//   Liczba?          int n = atoi(value);   (atoi("abc") = 0 – sprawdź n > 0, jeśli ma być dodatnia)
char* read_single_flag(int argc, char** argv, const char* all_flags, char wanted_flag)
{
    char* found_value = NULL;
    int option_character;
    const char* flag_in_all_flags = strchr(all_flags, wanted_flag);
    int flag_has_value = flag_in_all_flags != NULL && flag_in_all_flags[1] == ':';
    optind = 1;
    while ((option_character = getopt(argc, argv, all_flags)) != -1)
    {
        if (option_character == '?')
            usage(argv[0]);
        if (option_character == wanted_flag)
        {
            if (found_value != NULL)  // już raz była
            {
                fprintf(stderr, "Option -%c given more than once\n", wanted_flag);
                usage(argv[0]);
            }
            found_value = flag_has_value ? optarg : "";
        }
    }
    return found_value;
}

// ===== PRZYKŁAD UŻYCIA =====
#define ALL_FLAGS "p:n:v"

int main(int argc, char** argv)
{
    // -p może być wiele razy, co najmniej raz
    char* paths[argc];
    int paths_count = read_repeated_flag(argc, argv, ALL_FLAGS, 'p', paths);
    if (paths_count == 0)
        usage(argv[0]);

    // -n dokładnie raz, dodatnia liczba
    char* number_text = read_single_flag(argc, argv, ALL_FLAGS, 'n');
    if (number_text == NULL)
        usage(argv[0]);
    int number = atoi(number_text);
    if (number <= 0)
        usage(argv[0]);

    // -v opcjonalne, bez wartości
    int verbose = read_single_flag(argc, argv, ALL_FLAGS, 'v') != NULL;

    for (int i = 0; i < paths_count; i++)
        printf("path: %s\n", paths[i]);
    printf("number: %d\n", number);
    printf("verbose: %d\n", verbose);
    return EXIT_SUCCESS;
}
