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
    FILE *f = fopen(filename, "w+b"); //abre ou cria um arquivo filename, com leitura e escrita, tratado como binario

    if(f == NULL)
    {
        printf("Não foi possível criar o arquivo em create\n");
        return -1;
    }

    int i = 0;   //biblioteca vazia
    long offset = sizeof(int) + sizeof(long); //onde o diretório começa

    fseek(f, 0, SEEK_SET);  //garante a posição 0, início do arquivo
    fwrite(&i, sizeof(int),1,f); //escreve 4 bytes de i no arquivo (i = 0)

    fseek(f,sizeof(int), SEEK_SET);  //vou para o inicio do arquivo, pulo a count
    fwrite(&offset, sizeof(long),1,f); //escrevo 8 bytes de offset no fim do arquivo

    return fclose(f);
}

int gbv_open(Library *lib, const char *filename)
{
    if(lib == NULL || filename == NULL)
    {
        printf("Biblioteca ou arquivo nulo em open\n");
        return -1;
    }

    FILE *f = fopen(filename, "rb"); //abre para a leitura no modo binário

    //se não existe um arquivo, cria um novo 
    if(f == NULL)
    {
        if(gbv_create(filename) != 0)
            return -1;
        
        //abre o arquivo criado
        f = fopen(filename, "rb");
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

    fseek(f, sizeof(int), SEEK_SET); //vai para o ínicio do arquivo
    fread(&offset, sizeof(long),1, f); // le 4 bytes da posição 0 e guarda em i

    if(lib->count >0)
    {
        lib->docs = (Document *) malloc(lib->count * sizeof(Document)); //aloca o vetor para guardar os documentos de lib->count
        //verifica se deu certo a alocação do vetor
        if(lib->docs == NULL)
        {
            printf("Erro na alocação de memória\n");
            fclose(f);
            return -1;
        }

        fseek(f, offset, SEEK_SET); //vou para o diretório
        fread(lib->docs, sizeof(Document), lib->count, f); //guardo, em endereço de lib->docs os documentos de lib->count
    }
    else
        lib->docs = NULL;

    return fclose(f);

}

int gbv_add(Library *lib, const char *archive, const char *docname) //comparar se o archive == docname
{
    if(lib == NULL || archive == NULL || docname == NULL)
    {
        printf("Não foi possível adicionar, erro no ponteiro em add\n");
        return -1;
    }

    if(strcmp(archive, docname) == 0)
    {
        printf("Erro, não é possível adicionar a biblioteca dentro dela\n");
        return -1;
    }

    int pos = -1;
    //verifica se o documento já existe, compara as strings dos nomes
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
        //faço o shift das posições (apaga em memoria)
        
        lib->count--;

        if(lib->count >0)
            lib->docs = (Document *)realloc(lib->docs, lib->count * sizeof(Document));
            //realoco com a quantidade de documentos agora
        else
        {
            free(lib->docs);
            lib->docs = NULL;
        }
    }

    FILE *d_file = fopen(docname, "rb"); //abro o arquivo a ser inserido
    if(d_file == NULL)
    {
        printf("Erro ao abrir o documento em add\n");
        return -1;
    }

    fseek(d_file, 0, SEEK_END); //vou para o fim do arquivo
    long tam_d = ftell(d_file);  //fala a posição (tamanho) do arquivo
    rewind(d_file); //volta para o inicio

    FILE *f = fopen(archive, "r+b");
    if(f == NULL)
    {
        fclose(d_file);
        printf("Não foi possível abrir a biblioteca em add\n");
        return -1;
    }

    //leio o superbloco atual
    int contagem;
    long offset;

    fseek(f,0,SEEK_SET); //vou para o inicio do superbloco
    fread(&contagem, sizeof(int), 1, f); // coloco, em contagem, os 4 primeiros bytes
    fread(&offset, sizeof(long),1,f); // coloco, em offset, os 8 bytes depois dos 4 primeiros

    long novo_offset = sizeof(int) + sizeof(long); //começa em 12 (depois do superbloco)

    if(contagem >0)
    {
        Document *temp = (Document *)malloc(contagem * sizeof(Document)); 
        //cria um vetor temporario para saber onde a area de dados termina
        if(!temp)
        {
            printf("Falha na alocação em add\n");
            fclose(d_file);
            fclose(f);
            return -1;
        }

        fseek(f,offset, SEEK_SET);  //aponto para a posição offset
        fread(temp, sizeof(Document), contagem, f);// coloco, no vetor temp, os documentos

        long max_aux = novo_offset;

        //para cada documento, calcula onde ele termina
        //o maior fim é o fim da área de dados, onde o novo doc começa
        for (int i = 0; i<contagem; i++)
        {
            long fim = temp[i].offset + temp[i].size;

            if(fim > max_aux)
                max_aux = fim;
        }

        novo_offset = max_aux;
        free(temp);
    }

    fseek(f, novo_offset, SEEK_SET); //aponto para o novo fim

    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_leitura;

    //copia o documento em blocos para o .gbv
    while((bytes_leitura = fread(buffer,1,BUFFER_SIZE, d_file))> 0)
        fwrite(buffer, 1, bytes_leitura, f);

    //cria novo documento
    Document novo;
    strcpy(novo.name, docname);
    novo.size = tam_d;
    novo.date = time(NULL);
    novo.offset = novo_offset;

    lib->count++;
    //realoca o ponteiro docs para caber o documento a ser inserido
    lib->docs = (Document *)realloc(lib->docs, lib->count * sizeof(Document));

    if(!lib->docs)
    {
        printf("Erro na realocação em add\n");
        fclose(d_file);
        fclose(f);
        return -1;
    }

    //adiciona no final do vetor
    lib->docs[lib->count -1] = novo;

    //escrever diretorio no arquivo
    fseek(f, 0, SEEK_END); // aponto para o fim do arquivo
    long novo_d_offset = ftell(f);  //guarda a posicao do fim em f
    fwrite(lib->docs, sizeof(Document), lib->count, f); //escreve todos os docs nessa posição

    //atualizar superbloco
    fseek(f, 0, SEEK_SET);
    int nova_contagem = lib->count;
    fwrite(&nova_contagem, sizeof(int), 1, f);//escreve a nova contagem
    fwrite(&novo_d_offset, sizeof(long), 1, f); //escreve o novo offset

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

    //tenta achar o arquivo
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

    //faz o shift (retirar da memoria)
    for(int i = pos; i<lib->count -1; i++)
        lib->docs[i] = lib->docs[i+1];

    lib->count--;
    if(lib->count >0)
    {
        //realoca o vetor para caber o número atual de documentos
        lib->docs = (Document *)realloc(lib->docs, lib->count * sizeof(Document));
        if(lib->docs == NULL)
        {
            printf("Erro na realocação em remove\n");
            return -1;
        }
    } // se não tiver mais documentos, dá free no ponteiro
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

//int gbv_view(const Library *lib, const char *docname) 
int gbv_view(const Library *lib, const char *archive, const char *docname)
{
    if(lib == NULL || archive == NULL || docname == NULL)
    {
        printf("Ponteiro nulo em view\n");
        return -1;
    }

    int pos = -1;

    for (int i = 0; i < lib->count; i++)
    {
        if(strcmp (lib->docs[i].name, docname) == 0)
        {
            pos = i;
            break;
        }
    }

    if(pos == -1)
    {
        printf("Arquivo não achado em view\n");
        return -1;
    }

    FILE *f = fopen(archive, "rb");
    if(!f)
    {
        printf("Erro ao abrir a biblioteca em view\n");
        return -1;
    }

    Document doc = lib->docs[pos]; //documento auxiliar que recebe o documento em questão
    long aux_offset = doc.offset;  //offset do doc (posição)
    long aux_size = doc.size; //size do doc

    int tblocos = (aux_size + BUFFER_SIZE -1) / BUFFER_SIZE; //calcula quantos blocos(buffer)são necessarios p guardar o doc
    int bloco_atual = 1; //começa no primeiro bloco
    long aux_pos = 0; //quantos bytes foram lidos

    unsigned char buffer[BUFFER_SIZE];

    fseek(f, aux_offset + aux_pos, SEEK_SET); //aponta para a posição do bloco a ser lido
    size_t bytes_lidos = fread(buffer, 1, BUFFER_SIZE, f); 

    printf("Bloco %d/%d \n", bloco_atual, tblocos);
    fwrite(buffer, 1, bytes_lidos, stdout); // escreve na saida
    aux_pos += bytes_lidos; //vai para a posiçao do prox bloco
    bloco_atual++; //indica o proximo bloco

    char opcao;
    printf(" Escolha : n | p | q(sair) \n");
    scanf(" %c", &opcao);

    while (opcao != 'q')
    {
        if(opcao == 'n')
        {
            if(aux_pos >= aux_size)
            {
                printf("Ultimo bloco\n");
            }
            else
            {
                //se faltar menos que 512 bytes no bloco, lê só o que falta, se mais, lê 512 completos
                size_t ler = (aux_size - aux_pos < BUFFER_SIZE)? (aux_size - aux_pos) : BUFFER_SIZE;
                fseek(f, aux_offset + aux_pos, SEEK_SET); //vai para o inicio do bloco a ser lido
                bytes_lidos = fread(buffer,1, ler, f); //le quantos byes foram lidos e guarda em buffer

                printf("\n Bloco %d/%d", bloco_atual, tblocos);
                fwrite(buffer, 1, bytes_lidos, stdout); //escreve o conteudo do buffer na tela
                printf("\n");

                aux_pos += bytes_lidos; //vai para a proxima posicao relativa
                bloco_atual++; //vai para o proximo bloco

            }
        }
        else if(opcao == 'p')
        {
            if(bloco_atual == 1)
            printf("Voce esta no primeiro bloco\n");
            else
            {
                bloco_atual--; //volto um bloco

                long nova_pos = (bloco_atual -1) * BUFFER_SIZE; //calcula o endereço do bloco anterior
                if(nova_pos <0) 
                    nova_pos = 0;
                
                aux_pos = nova_pos;

                //vê se le 512 bytes ou menos
                size_t ler = (aux_size - aux_pos < BUFFER_SIZE)? (aux_size - aux_pos) : BUFFER_SIZE;
                fseek(f, aux_offset + aux_pos, SEEK_SET);
                bytes_lidos = fread(buffer, 1, ler, f);

                printf("Bloco %d/%d\n", bloco_atual, tblocos);
                fwrite(buffer,1,bytes_lidos, stdout);
                printf("\n");

                //atualiza a posição para o bloco depois do lido
                aux_pos += bytes_lidos;
            }
        }
        else
            printf("Comando invalido\n");

        printf("\n Opção: ");
        scanf(" %c", &opcao);
    }

    fclose(f);
    return 0;

}
