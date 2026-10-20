// like grep: returns first whole line (without '\n') containing pattern, searching from the beginning of file
// returned pointer is for newly allocated memory and must be freed. returns NULL if nothing matches
char* grep_first(FILE* file, const char* pattern)
{
    char* line = NULL;
    size_t len = 0;
    char* res = NULL;

    rewind(file);
    while (getline(&line, &len, file) != -1)
    {
        if (strstr(line, pattern))
        {
            line[strcspn(line, "\n")] = 0;
            res = strdup(line);
            if (!res)
                ERR("strdup");
            break;
        }
    }
    if (ferror(file))
        ERR("getline");
    free(line);
    return res;
}
