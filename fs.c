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
    int bloco_inicial;
    int size;

} dir_entry;


typedef struct{

  char modo;
  int bloco_inicial;
  int deslocamento;

} arquivo;

dir_entry dir[128];

arquivo arq[128];

int formatado = 1;

char buffer_rw[CLUSTERSIZE];

int fs_init() {

  int i =0,j=0;
  char *buffer;

  /*inicializar a struct diretorio*/
  for(i=0; i < 128;i++){
    dir[i].used = 0;
    dir[i].bloco_inicial = 0;
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
  dir[livre].bloco_inicial = 0;

  for (j = 256; j < FATSIZE && !dir[i].bloco_inicial; j++) {
    if (fat[j] == 1) {
      dir[i].bloco_inicial = j;
      fat[j] = 2;
    }
  }

  for (fBlock = 0, i = 256; i < FATSIZE; i++) {
    if (fat[i] == 1 && !fBlock) {
      dir[livre].bloco_inicial = i;
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
  j = dir[remove].bloco_inicial;

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
  int i = 0, flag_open = 0;
  /*Leitura */
  if(mode == FS_R){
    for(i = 0; i < 128; i++){
      if(strcmp(dir[i].name,file_name) == 0 && dir[i].used){

        arq[i].modo = FS_R;
        arq[i].bloco_inicial = dir[i].bloco_inicial;

        return i;
      }
    }
    if(i == 128) {
      printf("Arquivo não existe\n");
      return -1;
    }
  }
  /*Escrita */
  else if(mode == FS_W){

    for(i = 0; i < 128;i++){
      if(strcmp(dir[i].name,file_name) == 0){
        if(!fs_remove(file_name)){
          return -1;
        }
        if(!fs_create(file_name)){
          return -1;
        }

        flag_open = 1;
        arq[i].modo = FS_W;
        arq[i].bloco_inicial = dir[i].bloco_inicial;
        return i;
      }
    }
    if(i == 128 && !flag_open){
      if (!fs_create(file_name)) {
      	return -1;
      }
      flag_open = 1;
    }
    for(i = 0 ; i < 128; i++){

      if(dir[i].used == 0){
        fs_create(file_name);
        arq[i].modo = FS_W;
        arq[i].bloco_inicial = dir[i].bloco_inicial;
        return i;
      }
    }
  }

  return -1;
}

int fs_close(int file) {

    if(arq[file].modo == -1){
        printf("O arquivo já está fechado\n");
        return -1;
    }

    //Atualiza arquivos no disco
    for (int i = 0; i < (CLUSTERSIZE / SECTORSIZE); i++) {
		bl_write(arq[file].bloco_inicial * CLUSTERSIZE / SECTORSIZE + i, buffer_rw + i * SECTORSIZE);
	}

	// Escrita do arquivo
	for (int i = 0; i < 256; i++) {
		if (!bl_write(i, (char *) fat + i * SECTORSIZE)) {
			return -1;
		}
	}

  	// Escrita do diretório
	for (int i = 0; i < 8; i++){
		if (!bl_write(i + 8, (char *) dir + i * SECTORSIZE))
			return -1;
	}

    arq[file].modo = -1; //Usado para informar que está fechado  arquivo
    arq[file].bloco_inicial = 0;

    return 1;
}

int fs_write(char *buffer, int size, int file) { // FALTA TERMINAR!!!!!

  int pos_escrita = 0, deslocamento = 0, desloc_setor = 0, i;

  if (arq[file].bloco_inicial == 0) {
    printf("Erro: o arquivo encontra-se fechado.\n");
	return -1;
  }

  if (arq[file].modo != FS_W) {
  	printf("Erro: o arquivo não encontra-se em modo escrita.\n");
	return -1;
  }

  if (fs_free() < size) { // Verifica se o espaço livre em disco é suficiente
	printf("Não há espaço suficiente no disco.\n");
	return 0;
  }

  for (; (arq[file].deslocamento + size) > CLUSTERSIZE; size -= pos_escrita) {

  	desloc_setor = arq[file].deslocamento % SECTORSIZE; //Deslocamento por setor
  	pos_escrita += SECTORSIZE - desloc_setor;

  	strncpy(buffer_rw, buffer, pos_escrita); //Copia para o buffer o deslocamento atual

  	deslocamento += pos_escrita;
  }

  //Da flush do cluster no disco
  for (i = 0; i < CLUSTERSIZE / SECTORSIZE; i++) {
	if (!bl_write(arq[file].bloco_inicial * CLUSTERSIZE / SECTORSIZE + i, buffer + i * SECTORSIZE)) {
		printf("Não foi possível efetuar a escrita");
          return -1;
    }
  }

  pos_escrita += size;
  strncpy(buffer_rw, buffer, pos_escrita);

  return pos_escrita;
}

int fs_read(char *buffer, int size, int file) {
   int byte_count = 0, desloc = 0, i;

   if (arq[file].bloco_inicial == 0) {
      printf("Erro: o arquivo encontra-se fechado.\n");
      return -1;
   }

   if (arq[file].modo != FS_R) {
      printf("Erro: o arquivo não encontra-se em modo leitura.\n");
      return -1;
   }

   if (!arq[file].deslocamento) {
      for (i = 0; i < CLUSTERSIZE / SECTORSIZE; i++)
         bl_read(arq[file].bloco_inicial * CLUSTERSIZE / SECTORSIZE + i, buffer_rw + i * SECTORSIZE);
   }

   while (arq[file].deslocamento + size > CLUSTERSIZE) {
      if (arq[file].deslocamento < CLUSTERSIZE) {
         if(fat[arq[file].bloco_inicial] == 2)
            byte_count += (dir[file].size % CLUSTERSIZE - arq[file].deslocamento);
         else
            byte_count += (SECTORSIZE - (arq[file].deslocamento % SECTORSIZE));
         size -= byte_count;

         for (i = 0; i < byte_count; i++)
            *(buffer + desloc + i) = *(buffer_rw + arq[file].deslocamento + i);

         desloc += byte_count;
      }

      arq[file].deslocamento = (arq[file].deslocamento + byte_count) % CLUSTERSIZE;

      if (!arq[file].deslocamento){
         arq[file].bloco_inicial = fat[arq[file].bloco_inicial];
         for (i = 0; i < CLUSTERSIZE / SECTORSIZE; i++)
            bl_read(arq[file].bloco_inicial * CLUSTERSIZE / SECTORSIZE + i, buffer_rw + i * SECTORSIZE);
      }
   }

   if(arq[file].deslocamento == (dir[file].size % CLUSTERSIZE) && fat[arq[file].bloco_inicial] == 2)
      return byte_count;

   if (fat[arq[file].bloco_inicial] == 2) {
      if(dir[file].size % CLUSTERSIZE > size + arq[file].deslocamento)
         byte_count += size;
      else
         byte_count += (dir[file].size % CLUSTERSIZE - arq[file].deslocamento);

      for (i = 0; i < byte_count - desloc; i++)
         *(buffer + desloc + i) = *(buffer_rw + arq[file].deslocamento + i);

      arq[file].deslocamento = (arq[file].deslocamento + byte_count - desloc) % CLUSTERSIZE;
   }
   else {
      byte_count += size;
      for (i = 0; i < size; i++)
         *(buffer + desloc + i) = *(buffer_rw + arq[file].deslocamento + i);

      arq[file].deslocamento = (arq[file].deslocamento + size) % CLUSTERSIZE;
   }

   if (!arq[file].deslocamento){
      arq[file].bloco_inicial = fat[arq[file].bloco_inicial];
      for (i = 0; i < CLUSTERSIZE / SECTORSIZE; i++)
         bl_read(arq[file].bloco_inicial * CLUSTERSIZE / SECTORSIZE + i, buffer_rw + i * SECTORSIZE);
   }

   return byte_count;
}

