#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "gbv.h"
#include "util.h"

int gbv_create(const char *filename)
{
    if(filename == NULL)
    {
        printf("Ponteiro nulo\n");
        return -1;
    }
    FILE *f = fopen(filename, "w+b");

    if(f == NULL)
    {
        printf("Não foi possível criar o arquivo\n");
        return -1;
    }

    int i = 0;   //biblioteca vazia
    long offset = sizeof(int) + sizeof(long);

    fseek(f, 0, SEEK_SET);  //garante a posição 0
    fwrite(&i, sizeof(int),1,f);

    fseek(f,sizeof(int), SEEK_SET);  //posição em multiplos de 4
    fwrite(&offset, sizeof(long),1,f);

    return fclose(f);
}

int gbv_open(Library *lib, const char *filename)
{
    if(lib == NULL || filename == NULL)
    {
        printf("Biblioteca ou arquivo nulo, não foi possível abrir\n");
        return -1;
    }

    FILE *f = fopen(filename, "rb");

    //se não existe um arquivo, cria um novo 
    if(f == NULL)
    {
        if(gbv_create(filename) != 0)
            return -1;
        
        //abre o arquivo criado
        f = fopen(filename, "r+b");
        if(f == NULL)
        {
            printf("Não foi possível abrir a biblioteca\n");
            return -1;
        }
    }

    //para ler o bloco
    int i;
    long offset;

    fseek(f,0, SEEK_SET);
    fread(&i, sizeof(int), 1, f);
    lib->count = i;

    fseek(f, sizeof(int), SEEK_SET);
    fread(&offset, sizeof(long),1, f);

    if(lib->count >0)
    {
        lib->docs = (Document *) malloc(lib->count * sizeof(Document));
        //verifica se deu certo a alocação do vetor
        if(lib->docs == NULL)
        {
            printf("Erro na alocação de memória\n");
            fclose(f);
            return -1;
        }

        fseek(f, offset, SEEK_SET);
        fread(lib->docs, sizeof(Document), lib->count, f);
    }
    else
        lib->docs = NULL;

    return fclose(f);

}

int gbv_add(Library *lib, const char *archive, const char *docname) //comparar se o archive == docname
{
    if(lib == NULL || archive == NULL || docname == NULL)
    {
        printf("Não foi possível adicionar, erro no ponteiro\n");
        return -1;
    }

    int pos = -1;
    for (int i = 0; i<lib->count; i++)
    {
        if(strcmp(lib->docs[i].name, docname) == 0)
        {
            pos = i;
            break;
        }
    }

    if(pos != -1)
    {
        for(int i = pos; i<lib->count -1; i++)
            lib->docs[i] = lib->docs[i+1];
        
        lib->count--;

        if(lib->count >0)
            lib->docs = (Document *)realloc(lib->docs, lib->count * sizeof(Document));
        else
        {
            free(lib->docs);
            lib->docs = NULL;
        }
    }

    FILE *d_file = fopen(docname, "rb");
    if(d_file == NULL)
    {
        printf("Erro ao abrir o documento\n");
        return -1;
    }

    fseek(d_file, 0, SEEK_END);
    long tam_d = ftell(d_file);
    rewind(d_file);

    FILE *f = fopen(archive, "r+b"); //ver se não é rb+
    if(f == NULL)
    {
        fclose(d_file);
        printf("Não foi possível abrir a biblioteca\n");
        return -1;
    }

    int contagem;
    long offset;

    fseek(f,0,SEEK_SET);
    fread(&contagem, sizeof(int), 1, f);
    fread(&offset, sizeof(long),1,f);

    long novo_offset = sizeof(int) + sizeof(long);

    if(contagem >0)
    {
        Document *temp = (Document *)malloc(contagem * sizeof(Document));
        if(!temp)
        {
            printf("Falha na alocação\n");
            fclose(d_file);
            fclose(f);
            return -1;
        }

        fseek(f,offset, SEEK_SET);
        fread(temp, sizeof(Document), contagem, f);

        long max_aux = novo_offset;
        for (int i = 0; i<contagem; i++)
        {
            long fim = temp[i].offset + temp[i].size;

            if(fim > max_aux)
                max_aux = fim;
        }

        novo_offset = max_aux;
        free(temp);
    }

    fseek(f, novo_offset, SEEK_SET);

    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_leitura;

    while((bytes_leitura = fread(buffer,1,BUFFER_SIZE, d_file))> 0)
        fwrite(buffer, 1, bytes_leitura, f);

    Document novo;
    strcpy(novo.name, docname);
    novo.size = tam_d;
    novo.date = time(NULL);
    novo.offset = novo_offset;

    lib->count++;
    lib->docs = (Document *)realloc(lib->docs, lib->count * sizeof(Document));

    if(!lib->docs)
    {
        printf("Erro na realocação em add\n");
        fclose(d_file);
        fclose(f);
        return -1;
    }

    lib->docs[lib->count -1] = novo;

    fseek(f, 0, SEEK_END);
    long novo_d_offset = ftell(f);
    fwrite(lib->docs, sizeof(Document), lib->count, f);

    fseek(f, 0, SEEK_SET);
    int nova_contagem = lib->count;
    fwrite(&nova_contagem, sizeof(int), 1, f);
    fwrite(&novo_d_offset, sizeof(long), 1, f);

    fclose(d_file);
    fclose(f);

    printf("Documento adicionado\n");

    return 0;
}

int gbv_remove(Library *lib, const char *docname)
{
    if(lib == NULL || docname == NULL)
    {
        printf("Ponteiro nulo em remove\n");
        return -1;
    }

    int pos = -1;

    for(int i = 0; i<lib->count; i++)
    {
        if(strcmp(lib->docs[i].name, docname) == 0)
        {
            pos = i;
            break;
        }
    }

    if(pos == -1)
    {
        printf("Documento não encontrado em remove\n");
        return -1;
    }

    for(int i = pos; i<lib->count -1; i++)
        lib->docs[i] = lib->docs[i+1];

    lib->count--;
    if(lib->count >0)
    {
        lib->docs = (Document *)realloc(lib->docs, lib->count * sizeof(Document));
        if(lib->docs == NULL)
        {
            printf("Erro na realocação\n");
            return -1;
        }
    }
    else
    {
        free(lib->docs);
        lib->docs = NULL;
    }

    printf("Documento removido\n");
    return 0; 
}

int gbv_list(const Library *lib)
{
    if(lib == NULL)
    {
        printf("Ponteiro nulo em list\n");
        return -1;
    }

    if(lib->count == 0)
    {
        printf("Nenhum arquivo na biblioteca\n");
        return 0;
    }

    printf("-------Lista-de-Arquivos-------\n");

    for(int i = 0; i < lib->count; i++)
    {
        char data[30];
        format_date(lib->docs[i].date, data, sizeof(data));

        printf("ARQUIVO\n");
        printf("Nome: %s\n", lib->docs[i].name);
        printf("Tamanho: %ld bytes\n", lib->docs[i].size);
        printf("Data: %s\n", data);
        printf("Offset: %ld\n", lib->docs[i].offset);
        printf("----------\n");

    }
    return 0;
}

int gbv_view(const Library *lib, const char *docname)
{
    if(lib == NULL || docname == NULL)
    {
        printf("Ponteiro nulo em view\n");
        return -1;
    }

    
}
