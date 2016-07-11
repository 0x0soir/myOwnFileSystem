#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pagemap.h>
#include <linux/vfs.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <linux/ctype.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jorge Parrilla");

#define LFS_MAGIC 0x070162
#define TMPSIZE 50

/****************************************
*			FUNCIONES DEFINIDAS			*
****************************************/
struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags);
static int assoofs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
static int assoofs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
static struct inode *assoofs_make_inode(struct super_block *sb, int mode);
static int assoofs_open(struct inode *inode, struct file *filp);
static ssize_t assoofs_read_file(struct file *filp, char *buf, size_t count, loff_t *offset);
static ssize_t assoofs_write_file(struct file *filp, const char *buf, size_t count, loff_t *offset);
static struct dentry *assoofs_create_dir (struct super_block *sb, struct dentry *parent, const char *name);
static struct dentry *assoofs_create_file(struct super_block *sb, struct dentry *dir, const char *name, atomic_t *counter);
static void assoofs_create_files (struct super_block *sb, struct dentry *root);
static int assoofs_fill_super (struct super_block *sb, void *data, int silent);
static struct dentry *assoofs_get_super( struct file_system_type *fst, int flags, const char *devname, void *data);
struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags);
static int assoofs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
static int assoofs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
static int __init init_assoofs(void);
static void __exit cleanup_assoofs(void);
static ssize_t assoofs_write_file_counter(struct file *filp, const char *buf, size_t count, loff_t *offset);
static atomic_t counter, subcounter, contadorNuevos;
static char charsDebug[TMPSIZE];

/****************************************
*			STRUCT OPERACIONES			*
****************************************/
static struct file_system_type assoofs_type = {
	.owner		= THIS_MODULE,
	.name		= "assoofs",
	.mount 		= assoofs_get_super,
	.kill_sb 	= kill_litter_super,
};
static struct file_operations assoofs_file_ops = {
	.open = assoofs_open,
	.read = assoofs_read_file,
	.write = assoofs_write_file,
};
static struct super_operations assoofs_s_ops = {
	.statfs 	= simple_statfs,
	.drop_inode	= generic_delete_inode,
};
static struct inode_operations assoofs_inode_ops = {
	.create 	= assoofs_create,
	.lookup 	= assoofs_lookup,
	.mkdir 		= assoofs_mkdir,
};

/****************************************
*			CARGAR MODULO				*
****************************************/
static int __init init_assoofs(void)
{
	printk(KERN_INFO "-- ASSOOFS: Inicia el registro del sistema de ficheros en VFS\n");
	return register_filesystem(&assoofs_type);
}

static void __exit cleanup_assoofs(void)
{
	printk(KERN_INFO "-- ASSOOFS: Elimina el registro del sistema de ficheros en VFS\n");
	unregister_filesystem(&assoofs_type);
}

/****************************************
*			INICIO FILESYSTEM			*
****************************************/
static struct dentry *assoofs_get_super( struct file_system_type *fst, int flags, const char *devname, void *data)
{
	printk(KERN_INFO "-- ASSOOFS: [LOG] Entra en assoofs_get_super\n");
	return mount_bdev(fst, flags, devname, data, assoofs_fill_super);
}

static int assoofs_fill_super (struct super_block *sb, void *data, int silent)
{
	struct inode *root;
	struct dentry *root_dentry;

	printk(KERN_INFO "-- ASSOOFS: [LOG] Entra en assoofs_fill_super\n");

	// Crear y configurar superbloque
	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = LFS_MAGIC;
	sb->s_op = &assoofs_s_ops;

	// Crear inodo invocando a la funcion assoofs
	root = assoofs_make_inode(sb, S_IFDIR | 0755);
	if(!root)
		goto out;
	// Asignar al inodo las dos estructuras con operaciones soportadas
	//root->i_op = &simple_dir_inode_operations;

	root->i_op = &assoofs_inode_ops;
	root->i_fop = &simple_dir_operations;

	// Como el inodo raiz es un directorio se añade la estructura para que VFS lo localice
	root_dentry = d_make_root(root);

	if(!root_dentry)
		goto out_input;

	sb->s_root = root_dentry;
	assoofs_create_files(sb, root_dentry);

	return 0;

	out_input:
		iput(root);
	out:
		return -ENOMEM;
}

static struct inode *assoofs_make_inode(struct super_block *sb, int mode)
{
	struct inode *ret = new_inode(sb);
	if (ret) {
		ret->i_mode = mode;
		ret->i_uid.val = ret->i_gid.val = 0;
		ret->i_blocks = 0;
		ret->i_atime = ret->i_mtime = ret->i_ctime = CURRENT_TIME;
	}
	return ret;
}

/****************************************
*			CREAR FICHEROS				*
****************************************/
static void assoofs_create_files (struct super_block *sb, struct dentry *root)
{
	struct dentry *subdir;
	printk(KERN_INFO "-- ASSOOFS: [LOG] Entra en assoofs_create_files\n");

	atomic_set(&counter, 0);
	assoofs_create_file(sb, root, "contador1", &counter);

	subdir = assoofs_create_dir(sb, root, "carpeta1");

	atomic_set(&subcounter, 0);
	assoofs_create_file(sb, subdir, "contador2", &subcounter);
}

static struct dentry *assoofs_create_file (struct super_block *sb, struct dentry *dir, const char *name, atomic_t *counter)
{
	struct dentry *dentry;
	struct inode *inode;
	struct qstr qname;

	printk(KERN_INFO "-- ASSOOFS: [LOG] Entra en assoofs_create_file\n");

	qname.name = name;
	qname.len = strlen(name);
	qname.hash = full_name_hash(name, qname.len);

	dentry = d_alloc(dir, &qname);
	if (!dentry)
		goto out;

	inode = assoofs_make_inode(sb, S_IFREG | 0644);

	if (!inode)
		goto out_dput;
	inode->i_fop = &assoofs_file_ops;
	inode->i_private = counter;

	d_add(dentry, inode);
	return dentry;

	out_dput:
		dput(dentry);
	out:
		return 0;
}

static struct dentry *assoofs_create_dir (struct super_block *sb, struct dentry *parent, const char *name)
{
	struct dentry *dentry;
	struct inode *inode;
	struct qstr qname;

	printk(KERN_INFO "-- ASSOOFS: [LOG] Entra en assoofs_create_dir\n");

	qname.name = name;
	qname.len = strlen (name);
	qname.hash = full_name_hash(name, qname.len);
	dentry = d_alloc(parent, &qname);

	if (!dentry)
		goto out;

	inode = assoofs_make_inode(sb, S_IFDIR | 0755);
	if (!inode)
		goto out_dput;
	inode->i_op = &assoofs_inode_ops;
	inode->i_fop = &simple_dir_operations;

	d_add(dentry, inode);
	return dentry;

	out_dput:
		dput(dentry);
	out:
		return 0;
}

/****************************************
*			MANEJAR FICHEROS			*
****************************************/
static int assoofs_open(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "-- ASSOOFS: [LOG] Entra en assoofs_open\n");
	filp->private_data = inode->i_private;
	return 0;
}

static ssize_t assoofs_read_file(struct file *filp, char *buf, size_t count, loff_t *offset)
{
	struct inode *inode = filp->f_inode;
	atomic_t *counter;
	int v, len = 0;
	char tmp[TMPSIZE];
	printk(KERN_INFO "-- ASSOOFS: [LOG] Entra en assoofs_read\n");
	//printk(KERN_INFO "%s", &charsDebug[0]);
	if(inode->i_flags==0){
		counter = (atomic_t *) filp->private_data;
		v = atomic_read(counter);
		if(*offset>len)
			v-=1;
		else
			atomic_inc(counter);
		// Devolver valor al espacio de usuario
		len = snprintf(tmp, TMPSIZE, "%d\n", v);
	} else {
		len = snprintf(tmp, TMPSIZE, "%s", &charsDebug[0]);
	}
	if (*offset > len)
		return 0;
	if (count > len - *offset)
		count = len - *offset;

	if (copy_to_user(buf, tmp + *offset, count))
		return -EFAULT;
	*offset += count;
	return count;
}

static ssize_t assoofs_write_file(struct file *filp, const char *buf, size_t count, loff_t *offset)
{
	struct inode *inode = filp->f_inode;
	char tmp[TMPSIZE];
	int i;
	bool numerico = true;
	printk(KERN_INFO "-- ASSOOFS: [LOG] Entra en assoofs_write %zu\n", count);

	// Solo escribir desde el principio
	if (*offset != 0){
		printk(KERN_INFO "-- ASSOOFS: [LOG] Entra en assoofs_write - ERROR OFFSET\n");
		return -1;
	}

	// Leer el valor del usuario
	if (count >= TMPSIZE){
		printk(KERN_INFO "-- ASSOOFS: [LOG] Entra en assoofs_write - ERROR TMPSIZE\n");
		return -1;
	}
	memset(tmp, 0, TMPSIZE);
	if (copy_from_user(tmp, buf, count)){
		printk(KERN_INFO "-- ASSOOFS: [LOG] Entra en assoofs_write - ERROR COPY FROM USER\n");
		return -1;
	}
	for(i = 0; i<count-1; i++){
		if(!isdigit(tmp[i])){
			numerico = false;
			break;
		}
	}
	if(numerico){
		printk(KERN_INFO "-- ASSOOFS: [LOG] Es numero, se guarda como contador %s\n", &tmp[0]);
		count = assoofs_write_file_counter(filp, buf, count, offset);
		inode->i_flags = 0;
	} else {
		printk(KERN_INFO "-- ASSOOFS: [LOG] No es numero, se guarda como texto en global %s\n", &tmp[0]);
		copy_from_user(charsDebug, buf, count);
		inode->i_flags = 1;
	}

	return count;
}

static ssize_t assoofs_write_file_counter(struct file *filp, const char *buf, size_t count, loff_t *offset)
{
	atomic_t *counter = (atomic_t *) filp->private_data;
	char tmp[TMPSIZE];
	printk(KERN_INFO "-- ASSOOFS: [LOG] Entra en assoofs_write_file_counter %zu\n", count);

	// Solo escribir desde el principio
	if (*offset != 0){
		printk(KERN_INFO "-- ASSOOFS: [LOG] Entra en assoofs_write_file_counter - ERROR OFFSET\n");
		return -1;
	}

	// Leer el valor del usuario
	if (count >= TMPSIZE){
		printk(KERN_INFO "-- ASSOOFS: [LOG] Entra en assoofs_write_file_counter - ERROR TMPSIZE\n");
		return -1;
	}
	memset(tmp, 0, TMPSIZE);
	if (copy_from_user(tmp, buf, count)){
		printk(KERN_INFO "-- ASSOOFS: [LOG] Entra en assoofs_write_file_counter - ERROR COPY FROM USER\n");
		return -1;
	}

	atomic_set(counter, simple_strtol(tmp, NULL, 10));
	return count;
}


/****************************************
*			PARTE OPCIONAL				*
****************************************/
struct dentry *assoofs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags)
{
	printk(KERN_INFO "-- ASSOOFS: [LOG] Entra en assoofs_lookup\n");
	return NULL;
}

static int assoofs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	struct inode *inode;
	printk(KERN_INFO "-- ASSOOFS: [LOG] Entra en assoofs_mkdir\n");

	if (!dentry)
		goto out;

	inode = assoofs_make_inode(dir->i_sb, S_IFDIR | 0755);
	if (!inode)
		goto out_dput;
	inode->i_op = &assoofs_inode_ops;
	inode->i_fop = &simple_dir_operations;

	d_add(dentry, inode);
	inc_nlink(dir);
	return 0;

	out_dput:
		dput(dentry);
	out:
		return 0;

}

static int assoofs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
	struct inode *inode;
	printk(KERN_INFO "-- ASSOOFS: [LOG] Entra en assoofs_create\n");

	inode = assoofs_make_inode(dir->i_sb, S_IFREG | 0644);

	if (!inode)
		goto out_dput;
	inode->i_fop = &assoofs_file_ops;
	inode->i_private = &contadorNuevos;

	d_add(dentry, inode);
	return 0;

	out_dput:
		dput(dentry);
		return -1;
}

module_init(init_assoofs);
module_exit(cleanup_assoofs);
