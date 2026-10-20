// =====================================================================================================
// SOP1 L1 – przechodzenie po flagach uruchomienia (getopt). Działający program: skompiluj i poeksperymentuj.
// -----------------------------------------------------------------------------------------------------
// Kompilacja:  gcc -Wall -Wextra -fsanitize=address,undefined -o args sop-l1-args.c
// Przykład:    ./args -p dir1 -p dir2 -n 5 -o out.txt -v extra1 extra2
//
// Jak przerobić pod zadanie (3 miejsca):
//   1. OPTION_STRING – litery flag; ':' PO literze = flaga wymaga wartości ("p:" → -p ścieżka), bez ':' = flaga on/off ("v").
//   2. ProgramOptions – pola na wartości flag (+ domyślne wartości w parse_program_options).
//   3. switch w parse_program_options – co zrobić z każdą flagą.
//
// Na co uważać:
//   * Wartość flagi jest w optarg – to wskaźnik do argv, NIE kopiuj go, nie zwalniaj; żyje do końca programu.
//   * Po pętli argv[optind] .. argv[argc - 1] to argumenty BEZ flag (pozycyjne), np. "prog -v plik1 plik2".
//   * getopt akceptuje "-n5" i "-n 5", sklejone flagi "-vp dir" oraz "--" jako koniec flag.
//   * Na Linuksie (glibc) argumenty bez flag mogą stać też PRZED flagami ("prog x -p d1 y") – getopt przestawia je na koniec argv.
//   * atoi("abc") = 0 bez błędu – do liczb używaj parse_positive_number_or_exit (strtol ze sprawdzaniem).
//   * Kolejność flag jest dowolna; ta sama flaga może wystąpić kilka razy (tu -p zbiera się do tablicy).
//   * Funkcja usage NA KOŃCU robi exit(EXIT_FAILURE) – tak wymagają zadania przy złych argumentach.
// =====================================================================================================

#define _GNU_SOURCE

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

// ':' NA POCZĄTKU = getopt nie wypisuje własnych błędów i zwraca ':' przy brakującej wartości, '?' przy nieznanej fladze.
#define OPTION_STRING ":p:n:o:v"

// Wszystkie wartości z linii poleceń w jednym miejscu – przekazuj &options do funkcji etapów.
typedef struct
{
    char** input_paths;          // wszystkie wartości -p (tablica wskaźników do argv)
    int input_paths_count;       // ile razy podano -p
    int repeat_count;            // -n liczba (domyślnie 1)
    const char* output_path;     // -o plik (NULL = nie podano)
    int verbose_enabled;         // -v (0/1)
    char** positional_arguments; // argumenty bez flag (wskazuje do środka argv)
    int positional_count;
} ProgramOptions;

// usage(argv[0]) -> wypisuje poprawne użycie na stderr i KOŃCZY program kodem błędu. Dopasuj tekst do zadania.
void usage(const char* program_name)
{
    fprintf(stderr, "USAGE: %s -p path [-p path ...] [-n positive_number] [-o output_file] [-v] [extra ...]\n", program_name);
    fprintf(stderr, "  -p path    input path, can be repeated, at least one required\n");
    fprintf(stderr, "  -n number  positive number (default 1)\n");
    fprintf(stderr, "  -o file    output file (only once)\n");
    fprintf(stderr, "  -v         verbose\n");
    exit(EXIT_FAILURE);
}

// parse_positive_number_or_exit("12", 'n', argv[0]) -> 12; dla "abc", "12x", "", "0", "-3" albo za dużej liczby wywołuje usage (exit).
//   Zmień warunek "number_value <= 0", jeśli zadanie dopuszcza 0 albo ma zakres (np. 1..20).
int parse_positive_number_or_exit(const char* number_text, char option_letter, const char* program_name)
{
    char* first_invalid_character;
    errno = 0;
    long number_value = strtol(number_text, &first_invalid_character, 10);
    if (errno != 0 || first_invalid_character == number_text || *first_invalid_character != 0 || number_value <= 0 ||
        number_value > INT_MAX)
    {
        fprintf(stderr, "Invalid value for -%c: \"%s\"\n", option_letter, number_text);
        usage(program_name);
    }
    return (int)number_value;
}

// parse_program_options(argc, argv, &options) -> wypełnia options na podstawie flag; przy złych argumentach usage (exit).
//   Po wywołaniu: options.input_paths[0..input_paths_count-1], options.positional_arguments[0..positional_count-1].
//   Zwolnij potem tylko options.input_paths (free) – same stringi należą do argv.
void parse_program_options(int argc, char** argv, ProgramOptions* options)
{
    // wartości domyślne
    options->input_paths = malloc(argc * sizeof(char*));  // -p nie może wystąpić więcej razy niż jest argumentów
    if (!options->input_paths)
        ERR("malloc");
    options->input_paths_count = 0;
    options->repeat_count = 1;
    options->output_path = NULL;
    options->verbose_enabled = 0;

    int option_character;
    while ((option_character = getopt(argc, argv, OPTION_STRING)) != -1)
    {
        switch (option_character)
        {
            case 'p':  // powtarzalna flaga z wartością → zbieramy do tablicy
                options->input_paths[options->input_paths_count++] = optarg;
                break;
            case 'n':  // liczba z walidacją
                options->repeat_count = parse_positive_number_or_exit(optarg, 'n', argv[0]);
                break;
            case 'o':  // flaga z wartością, która może wystąpić tylko raz
                if (options->output_path != NULL)
                {
                    fprintf(stderr, "Option -o given more than once\n");
                    usage(argv[0]);
                }
                options->output_path = optarg;
                break;
            case 'v':  // flaga on/off
                options->verbose_enabled = 1;
                break;
            case ':':  // flaga wymaga wartości, a jej nie ma (np. "./args -p"); litera flagi jest w optopt
                fprintf(stderr, "Option -%c requires a value\n", optopt);
                usage(argv[0]);
                break;
            default:  // '?' = nieznana flaga (np. "-x")
                fprintf(stderr, "Unknown option -%c\n", optopt);
                usage(argv[0]);
        }
    }

    // wymagane flagi sprawdzamy PO pętli
    if (options->input_paths_count == 0)
    {
        fprintf(stderr, "At least one -p is required\n");
        usage(argv[0]);
    }

    // argumenty bez flag – zostają od optind do końca argv
    options->positional_arguments = argv + optind;
    options->positional_count = argc - optind;
}

// ===== WERSJA BEZ getopt – gdy zadanie mówi np. "prog n k p1 p2 ..." (same argumenty, bez myślników) =====
// iterate_arguments_manually(argc, argv) -> argv[0] to nazwa programu, argv[1..argc-1] to kolejne argumenty; sprawdź liczbę na starcie.
void iterate_arguments_manually(int argc, char** argv)
{
    if (argc < 2)  // dopasuj minimalną liczbę argumentów do zadania
        usage(argv[0]);
    for (int argument_index = 1; argument_index < argc; argument_index++)
        printf("argv[%d] = %s\n", argument_index, argv[argument_index]);
}

int main(int argc, char** argv)
{
    ProgramOptions options;
    parse_program_options(argc, argv, &options);

    printf("input paths (%d):\n", options.input_paths_count);
    for (int path_index = 0; path_index < options.input_paths_count; path_index++)
        printf("  %s\n", options.input_paths[path_index]);
    printf("repeat count: %d\n", options.repeat_count);
    printf("output path: %s\n", options.output_path ? options.output_path : "(none, use stdout)");
    printf("verbose: %s\n", options.verbose_enabled ? "yes" : "no");
    printf("positional arguments (%d):\n", options.positional_count);
    for (int argument_index = 0; argument_index < options.positional_count; argument_index++)
        printf("  %s\n", options.positional_arguments[argument_index]);

    free(options.input_paths);
    return EXIT_SUCCESS;
}
