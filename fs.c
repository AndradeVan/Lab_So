/*
 * RSFS - Really Simple File System
 *
 * Copyright © 2010 Gustavo Maciel Dias Vieira
 * Copyright © 2010 Rodrigo Rocco Barbieri
 *
 * This file is part of RSFS.
 *
 * RSFS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define CLUSTERSIZE 4096
#define FATSIZE 65536

unsigned short fat[FATSIZE];

typedef struct {
    char used;
    char name[25];
    unsigned short first_block;
    int size;
} dir_entry;

typedef struct{

  char modo;
  unsigned short offset;
  int carry;
  int blockNumber;

}arquivo;

dir_entry dir[128];

arquivo arq[128];

int formatado = 1;

int fs_init() {

  int i =0,j=0;
  char *buffer;
  
  /*inicializar a struct diretorio*/
  for(i=0; i < 128;i++){
    dir[i].used = 0;
    dir[i].first_block = 0;
    dir[i].name[0] = '\0';
    dir[i].size = 0;
  }
  buffer = (char *) fat;

  /*fazer leitura da FAT*/
  for(i=0; i < 256;i++){
    if(!bl_read(i,&buffer[i*SECTORSIZE]))
     return 0;
  }
  
  /*ler o diretorio do disco*/
  buffer = (char *) dir;
  for(i=256;i < 264;i++){
    if(!bl_read(i,&buffer[j*SECTORSIZE]))
      return 0;
    j++;
  } 
  //verificar se o disco está formatado
  for(i=0;i < 32;i++){
    if(fat[i] != 3){
      formatado = 0;
      break;
    }
  } 
  return 1;
}

int fs_format() {
  
  int i=0,j =0;
  char *buffer;

  for(i = 0; i < 32;i++){
    fat[i] = 3;
  }

  fat[32] = 4;
  
  for(i = 0; i < 128;i++){
    dir[i].used = 0;
  }

  for(i = 33;i < FATSIZE; i++){
    fat[i] = 1;
  }

  buffer = (char *) fat;
  for(i=0; i < 256;i++){
    if(!bl_write(i,&buffer[i*SECTORSIZE]))
     return 0;
  }
  
  buffer = (char *) dir;
  for(i=256;i < 264;i++){
    if(!bl_write(i,&buffer[j*SECTORSIZE]))
      return 0;
    j++;
  }

  formatado = 1;
  
  return 1;
}

int fs_free() {

  int i, cont = 0;

  for(i = 0 ; i < bl_size()/8; i++ ){
    if(fat[i] == 1){
      cont++;
    }
  }
  return cont*SECTORSIZE;
}

int fs_list(char *buffer, int size) {
 
  char aux[100];
  int flag = 0;

  for(int i=0; i < 128; i++){

    if(dir[i].used){

      strcat(buffer,dir[i].name);
      strcat(buffer,"\t\t");
      sprintf(aux, "%d", dir[i].size);
      strcat(buffer,aux);
      strcat(buffer,"\n");

      flag = 1;
    } 
  }
  return flag;
}

int fs_create(char* file_name) {
  
  int i = 0, j = 0,livre = -1, fBlock;
  char *buffer;

  if (!formatado) {
    perror("Disco não formatado");
    return 0;
  } 

  /*Verificar se o arquivo existe*/
  for(i=0; i < 128;i++){
    if(strcmp(dir[i].name,file_name) == 0 && dir[i].used){
      perror("Arquivo com o mesmo nome já existe");
      return 0;
    }
    if(dir[i].used == 0 && livre == -1){
      livre = i;
    }
  }

  if(livre == -1){
    printf("Não há espaço no diretorio\n");
    return 0;
  }

  if(strlen(file_name) > 25){
    perror("Nome de arquivo muito grande");
    return 0;
  }

  strcpy(dir[livre].name, file_name);
  dir[livre].used = 1; 
  dir[livre].size = 0;
  dir[livre].first_block = 0;

  for (j = 256; j < FATSIZE && !dir[i].first_block; j++) {
    if (fat[j] == 1) {
      dir[i].first_block = j;
      fat[j] = 2;
    }
  }  

  for (fBlock = 0, i = 256; i < FATSIZE; i++) {
    if (fat[i] == 1 && !fBlock) {
      dir[livre].first_block = i;
      fat[i] = 2;
      fBlock = 1;
    }
  }

  buffer = (char*) fat;
  for(i=0; i < 256; i++) {
    if(!bl_write(i,&buffer[i*SECTORSIZE])) {
      perror("Erro ao escrever a FAT no disco");
      return 0;
    }
  }

  buffer = (char*) dir;
  for(i = 256, j = 0;i < 264;i++){
    if(!bl_write(i,&buffer[j*SECTORSIZE])) {
      perror("Erro ao escrever o diretório no disco");
      return 0;
    }
    j++;
  }
  
  return 1;
}

int fs_remove(char *file_name) {

  int i,j;
  int remove = -1;
  char *buffer;

  /*Verificar se o arquivo existe*/
  for(remove = -1, i=0; i < 128;i++){
    if(strcmp(dir[i].name,file_name) == 0 && dir[i].used){
      remove = i;
    }
  }
  if(remove == -1){
    perror("Arquivo não existe");
    return 0;
  }

  strcpy(dir[remove].name, "");
  dir[remove].size = 0;
  dir[remove].used = 0;
  j = dir[remove].first_block;

  while(fat[j] != 2){ // remove tipo lista encadeada
    remove = j;
    j = fat[j];
    fat[remove] = 1;
  }

  fat[j] = 1;

  buffer = (char *) fat;
  for(i = 0; i < 256; i++){
    if(!bl_write(i, &buffer[i*SECTORSIZE]))
     return 0;
  }

  buffer = (char *) dir;
  for(i = 256, j = 0; i < 264; i++, j++){
    if(!bl_write(i, &buffer[j*SECTORSIZE]))
      return 0;
  }


  return 0;
}

/*Ate aqui */

int fs_open(char *file_name, int mode) {

  int i =0;
  /*Leitura */
  if(mode == FS_R){
    for(i =0; i < 128;i++){
      /*arquivo existe*/
      if(strcmp(dir[i].name,file_name) == 0 && dir[i].used){
        arq[i].modo = FS_R;
        arq[i].lastPos = 0;
        return i;   
      }
    }
    
    if(i == 128){
      printf("Arquivo não existe\n");
      return -1;
    }
  }
  /*Escrita */
  else if(mode == FS_W){
    for(i =0; i < 128;i++){
      if(strcmp(dir[i].name,file_name) == 0 && dir[i].used){
        fs_remove(*file_name);
        arq[i].modo = FS_W;
        arq[i].lastPos = 0;
        return i;
      }
    }
    if(i == 128){
      fs_create(*file_name);
    }  
  }
 
  return -1;
}

int fs_close(int file)  {
  printf("Função não implementada: fs_close\n");
  return 0;
}

int fs_write(char *buffer, int size, int file) {
 int contadorCluser = 0;

 //*Verificando se o arquivo existe*//
 if(arq[file]).first_block == 0){
   printf("Arquivo inexistente");
   return -1;
 }

 //*Verificação se arquivo é de escrita*//
 if(arq[file].modo != FS_W){
   printf("O arquivo não é um arquivo de escrita");
   return -1;
 }

contadorCluster = arq[file].contadorEscrita;
diretorio = arq[file].first_block;

}

int fs_read(char *buffer, int size, int file) {
  int posicao;

//*Verifica Existência do arquivo*//
if(arq[file].first_block = 0){
  printf("Arquivo Inexistente");
  return -1;

//*Verifica Se é arquivo de leitura*//
if(arq[file].modo != FS_R){
  printf("O arquivo não é um arquivo de leitura");
}

posicao = arq[file.first_block];

}


  printf("Função não implementada: fs_read\n");
  return -1;
}

