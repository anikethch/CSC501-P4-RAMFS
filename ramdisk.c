#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ramdisk.h"
#define NAME_LEN 128
#define DIR_SIZE 4096

unsigned long int disk_size,used_size;
node *root;
node *last;
int size_check;

node* traverse(const char *path) {
  node *tmp;
  tmp=root;
  while (1) {
    if (strcmp(path,tmp->name)==0) {
       break;
    } else {
       if (tmp->next !=NULL)  tmp = tmp->next;
       else return NULL;
    }
  }
  return tmp;
}

char* return_name(const char* str,int name) {

   const char ch = '/';
   char *ret;
   char *res;
   res = malloc(sizeof(char)*NAME_LEN);
   ret = strrchr(str, ch);
   int basename_size = strlen(ret);
   int act_size = strlen(str);
   int dirname_size;
   dirname_size = act_size-basename_size;
   strncpy(res,str,dirname_size);
   strcat(res,"\0");

   char *token=malloc(100);
   token=strtok(ret,"/");
   
   switch(name) {
     case 0 :
          return res;
     case 1:
          return token;
     default:
          return NULL;
   }
}

int find_child(const char *path) {
    int count=0;
    node *tmp;
    tmp = root;
    while(1) {
       char *tmp1;
       tmp1 = malloc(sizeof(char)*NAME_LEN);
       tmp1 = return_name(tmp->name,0);
       if (tmp->next!=NULL) tmp = tmp->next;
       else break;
    }
    return count;
}


int check_disk(int size) {
   if ((used_size+size)<=disk_size) {
      used_size = used_size+size;
      return 0;
   } else {
      return -ENOMEM;
   }
}

void remove_disk(int size) {
    used_size = used_size-size;
}

static int fs_open(const char *path,struct fuse_file_info *fi) {
  if(traverse(path)==NULL) return -ENOENT;
  return 0;
}

static int fs_write(const char *path,char *buf,size_t size,off_t offset) {
   node *tmp;
   char *data;
   int ret_val;
   tmp =  traverse(path); 
   if(tmp==NULL) return -ENOENT;
   if(!tmp->type) return -EINVAL;
   if((offset+size)>tmp->size) {
      data = (char *)malloc(sizeof(char)*(offset+size));
      ret_val=check_disk(sizeof(char)*(offset+size));
      if (ret_val!=0 && (offset+size)) return -ENOMEM;
      if ((offset+size) > tmp->size) {
          memset(data,"\0",offset+size);
          memcpy(data,tmp->data,tmp->size);
          free(tmp->data);
          remove_disk(tmp->size);
          tmp->data = data;
          tmp->size = offset+size;
          tmp->ctime = time(0);
          tmp->mtime = time(0);
      }
   }
   memcpy(tmp->data+offset,buf,size);  
   return size;
}

static int fs_truncate(const char *path,off_t size) {

   node *tmp;
   char *data;
   int ret_val;
   tmp = traverse(path);
   if (!tmp->type) return -EINVAL;
   if (tmp==NULL) return -ENOENT;
   if (tmp->size == size) return 0;
   data = (char *)malloc(sizeof(char)*size);
   ret_val=check_disk(sizeof(char)*size);
   if (ret_val!=0 && data ) return -ENOMEM;
   remove_disk(tmp->size);
   free(tmp->data);
   tmp->data = data;
   tmp->size = size;
   return 0;
}

static int fs_create(const char *path,mode_t mode) {
  node *tmp;
  tmp = (node *)malloc(sizeof(node));
  size_check=check_disk(sizeof(node));
  if (size_check!=0) return size_check;
  tmp->type = 1;
  tmp->mode = 0766 | S_IFREG;
  tmp->size = 0;
  tmp->name = (char *)malloc(sizeof(char)*NAME_LEN);
  size_check=check_disk(sizeof(char)*NAME_LEN);
  if (size_check!=0) return size_check;
  strcpy(tmp->name,path);
  strcat(tmp->name,"\0");
  tmp->data = NULL;
  tmp->next = NULL;
  last->next = tmp;
  last = tmp;
  tmp->atime = time(0);
  tmp->ctime = time(0);
  tmp->mtime = time(0);
  return 0;
}

static int fs_getattr(const char *path,struct stat *st_buf) {
  node *tmp;
  memset(st_buf,0,sizeof(struct stat));
  if (strcmp(path,"/")==0) {
     st_buf->st_mode = S_IFDIR | 0755;
  }  else {
     tmp = traverse(path);
     if (tmp==NULL) return -ENOENT;
     else {
        st_buf->st_nlink = 1;
        st_buf->st_mode  =  tmp->mode;
        st_buf->st_atime =  tmp->atime;
        st_buf->st_ctime =  tmp->ctime;        
        st_buf->st_mtime =  tmp->mtime;
        st_buf->st_size  =  tmp->size;
     }
  }
  return 0;
}

static int fs_mkdir(const char *path,mode_t mode) {
  size_check=check_disk(DIR_SIZE);
  if (size_check!=0) return size_check;
  node *tmp;
  tmp = (node *)malloc(sizeof(node));
  size_check=check_disk(sizeof(node));
  if (size_check!=0) return size_check;
  tmp->type = 0;
  tmp->mode = 0755 | S_IFDIR;
  tmp->size = DIR_SIZE;
  tmp->name = (char *)malloc(sizeof(char)*NAME_LEN);
  size_check=check_disk(sizeof(char)*NAME_LEN);
  if (size_check!=0) return size_check;
  strcpy(tmp->name,path);
  strcat(tmp->name,"\0");
  tmp->data = NULL;
  tmp->next = NULL;
  last->next = tmp;
  last = tmp;
  tmp->atime = time(0);
  tmp->ctime = time(0);
  tmp->mtime = time(0);
  return 0;
}

static int fs_rmdir(const char *path) {

   int child;
   child = find_child(path);
   if (child != 0) return -ENOTEMPTY;
   else {
     node *tmp,*tmp1;
     tmp = root;
     while (strcmp(path,tmp->next->name)!=0) {
       tmp = tmp->next;
     } 
     if (tmp->next == last) last = tmp;
     tmp1 = tmp->next;
     tmp->next =  tmp->next->next; 
     remove_disk(tmp1->size);
     remove_disk(sizeof(node));
     remove_disk(sizeof(char)*NAME_LEN);
     free(tmp1);
     return 0;
   }
}

static int fs_opendir(const char *path) {
  
   if(traverse(path)==NULL) return -ENOENT;
   return 0;
}

static int fs_read(const char *path,char *buf,size_t size,off_t offset) {
  node *tmp;
  tmp = traverse(path);
  if (tmp==NULL) return -ENOENT;
  if (tmp->type == 0) return -EINVAL;
  if (offset >= (tmp->size)) return 0;
  if (size > (tmp->size)-offset) size = (tmp->size)-offset; 
  memset(buf,0,size);
  memcpy(buf,(tmp->data)+offset,size);
  return size;
}

static int fs_readdir(const char *path,void *buf,fuse_fill_dir_t filler,off_t offset,struct fuse_file_info *fi) {

   filler(buf,".",NULL,0);
   filler(buf,"..",NULL,0);

   node *temp;
   temp = root;
   if (traverse(path)==NULL) return -ENOENT;
   while(temp!=NULL) {
      char *dir,*file;
      dir =  (char*)malloc(NAME_LEN);
      dir = return_name(temp->name,0);
      file =  (char*)malloc(NAME_LEN);
      file = return_name(temp->name,1);
      if (strcmp(dir,"")==0) strcpy(dir,"/");
      strcat(dir,"\0");
       if (strcmp(dir,path)==0) {
         if(file !=NULL)
         filler(buf,file,NULL,0);
      }
      temp = temp->next;
   }
   return 0;
}

static int fs_unlink(const char *path) {
     node *tmp;
     node *tmp1;
     tmp = root;
     while (strcmp(path,tmp->next->name)!=0) {
       tmp = tmp->next;
     }
     if (tmp->next == last) last = tmp;
     tmp1 =   tmp->next;
     tmp->next =  tmp->next->next;
     remove_disk(tmp1->size);
     remove_disk(sizeof(node));
     remove_disk(sizeof(char)*NAME_LEN);
     free(tmp1);
     return 0;
}

static  struct fuse_operations ramdisk_op = {
   .getattr = fs_getattr,
   .mkdir   = fs_mkdir,
   .readdir = fs_readdir,
   .rmdir   = fs_rmdir,
   .opendir = fs_opendir,
   .create  = fs_create,
   .unlink  = fs_unlink,
   .open    = fs_open,
   .read    = fs_read,
   .write   = fs_write,
   .truncate = fs_truncate,
};

int fs_initialize() {
   used_size = 0;
   root = (node *)malloc(sizeof(node));
   size_check = check_disk(sizeof(node));
   if (size_check!=0) return size_check;
   root->type = 0;
   root->mode = S_IFDIR | 0755;
   root->name = (char *)malloc(sizeof(char)*NAME_LEN);    
   size_check = check_disk(sizeof(char)*NAME_LEN);
   if (size_check!=0) return size_check;
   strcpy(root->name,"/");
   strcat(root->name,"\0");
   root->data = NULL;
   root->size = 0;
   root->next = NULL;
   root->atime = time(0);
   root->ctime = time(0);
   root->mtime = time(0);
   last = root; 
   size_check = check_disk(sizeof(node));
   if (size_check!=0) return size_check;
   size_check = check_disk(sizeof(char)*NAME_LEN);
   if (size_check!=0) return size_check;
   return 0;
}

int main (int argc, char* argv[]) {
  int fuse_argc,count;
  int ret_val;

  if(argc!=3) {
    fprintf(stderr,"Please enter all the arguments required\n");
    exit(-1);
  }

  disk_size = atoi(argv[2]);
  if (disk_size == 0)  {
     fprintf(stderr,"Please enter command in this format : ramdisk <path to mount dir> <size of ramdisk in MB>\n");
     exit(-1);
  }
  disk_size = disk_size*1024*1024;
  fuse_argc = argc-1;
  
  ret_val=fs_initialize();
  if (ret_val!=0) {
     printf("Please enter more size for ramdisk\n");
     exit(ret_val);
  }

  char *fuse_argv[fuse_argc];
  count = 0;
  for (count;count<=fuse_argc;count++) {
    fuse_argv[count]=argv[count];
  }
  umask(0); 
  return fuse_main(fuse_argc,fuse_argv,&ramdisk_op,NULL);
}
