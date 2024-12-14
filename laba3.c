#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#define procfs_name "tsu"

static struct proc_dir_entry *our_proc_file = NULL;

/* Функция чтения из файла /proc/tsu */
static ssize_t procfile_read(struct file *file_pointer, char __user *buffer, size_t buffer_length, loff_t *offset)
{
    char s[6] = "Tomsk\n";
    int len = sizeof(s);

    if (*offset > 0 || buffer_length < len)
    {
        return 0;
    }

    if (copy_to_user(buffer, s, len))
    {
        return -EFAULT;
    }

    pr_info("procfile read %s\n", file_pointer->f_path.dentry->d_name.name);
    *offset += len;

    return len;
}

/* Указатель на функцию чтения, зависит от версии ядра */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops proc_file_fops = {
    .proc_read = procfile_read,
};
#else
static const struct file_operations proc_file_fops = {
    .read = procfile_read,
};
#endif

/* Функция инициализации модуля */
static int __init procfs1_init(void)
{
    our_proc_file = proc_create(procfs_name, 0644, NULL, &proc_file_fops);
    if (our_proc_file == NULL)
    {
        pr_alert("Error: Could not initialize /proc/%s\n", procfs_name);
        return -ENOMEM;
    }
    pr_info("Loaded the TSU Linux Module\n");
    return 0;
}

/* Функция выгрузки модуля */
static void __exit procfs1_exit(void)
{
    proc_remove(our_proc_file);
    pr_info("Unloading the TSU Linux Module\n");
    pr_info("/proc/%s removed\n", procfs_name);
}

module_init(procfs1_init);
module_exit(procfs1_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("RC");
MODULE_DESCRIPTION("RC");
