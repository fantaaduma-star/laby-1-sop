char **paths = NULL;
    int n_paths = 0;
    char *outfile = NULL;
    int opt;
    FILE * out;
    while ((opt = getopt(argc, (char *const *)argv, "p:o:")) != -1) {
        switch (opt) {
            case 'p':
                paths = realloc(paths, (n_paths + 1) * sizeof(char *));
                if (!paths) ERR("realloc");
                paths[n_paths++] = optarg;
                break;
            case 'o':
                outfile = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s [-p path ...] [-o outfile]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
