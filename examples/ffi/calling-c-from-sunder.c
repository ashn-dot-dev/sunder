int x = 100;
int const y = 200;

extern int __sunder_main(int argc, char** argv, char** envp);

int
main(int argc, char** argv, char** envp)
{
    // Call the Sunder `main` function.
    __sunder_main(argc, argv, envp);
}
