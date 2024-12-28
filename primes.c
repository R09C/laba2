#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/string.h>

#define procfs_name "primes"
static int limit = 100; // По умолчанию 100

module_param(limit, int, 0644);
MODULE_PARM_DESC(limit, "Upper limit for primes search");

static struct proc_dir_entry *our_proc_file = NULL;

// Функция для решета Эратосфена
static char* eratosthenes(int limit) {
    bool* is_prime;
    char* primes_str;
    int i, j, count = 0, len = 0;

    // Выделяем память под флаги и проверяем успешность
    is_prime = kmalloc(sizeof(bool) * (limit + 1), GFP_KERNEL);
    if (!is_prime) {
        pr_err("Failed to allocate memory for is_prime\n");
        return NULL;
    }
    
    memset(is_prime, true, sizeof(bool) * (limit + 1));
    is_prime[0] = is_prime[1] = false;

    for (i = 2; i * i <= limit; i++) {
        if (is_prime[i]) {
            for (j = i * i; j <= limit; j += i) {
                is_prime[j] = false;
            }
        }
    }

    for (i = 2; i <= limit; i++) {
        if (is_prime[i]) {
        count++;
        
            // Оценка размера строки
            char temp[12];  // Максимальный размер для int
            int numlen = snprintf(temp, sizeof(temp), "%d ", i);
            len += numlen;
        }
    }


    if (count == 0){
        kfree(is_prime);
        primes_str = kmalloc(sizeof(char)*5, GFP_KERNEL);
        if (!primes_str)
        {
            pr_err("Failed to allocate memory for primes_str\n");
            return NULL;
        }
        snprintf(primes_str,5,"None\n");
        return primes_str;
    }


    // Выделяем память под строку простых чисел и проверяем успешность
    primes_str = kmalloc(sizeof(char) * (len + 2), GFP_KERNEL);  // +2 для '\n' и '\0'
    if (!primes_str) {
        pr_err("Failed to allocate memory for primes_str\n");
        kfree(is_prime);
        return NULL;
    }

    // Заполняем строку простыми числами
    int offset = 0;
    for (i = 2; i <= limit; i++) {
        if (is_prime[i]) {
            offset += snprintf(primes_str + offset, len - offset + 1, "%d ", i);
        }
    }
    primes_str[offset] = '\n';
    primes_str[offset+1] = '\0';


    kfree(is_prime);
    return primes_str;
}



/* Функция чтения из файла /proc/primes */
static ssize_t procfile_read(struct file *file_pointer, char __user *buffer, size_t buffer_length, loff_t *offset)
{
    char* primes_str;
    int len;

    if (*offset > 0) {
        return 0;
    }

    primes_str = eratosthenes(limit);
    if (!primes_str) {
        return -ENOMEM;
    }

    len = strlen(primes_str);

    if (buffer_length < len)
    {
        kfree(primes_str);
        return -EFAULT;
    }


    if (copy_to_user(buffer, primes_str, len)) {
        kfree(primes_str);
        return -EFAULT;
    }

    kfree(primes_str);

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
    if (our_proc_file == NULL) {
        pr_alert("Error: Could not initialize /proc/%s\n", procfs_name);
        return -ENOMEM;
    }
    pr_info("Loaded the Primes Linux Module with limit %d\n", limit);
    return 0;
}

/* Функция выгрузки модуля */
static void __exit procfs1_exit(void)
{
    proc_remove(our_proc_file);
    pr_info("Unloading the Primes Linux Module\n");
    pr_info("/proc/%s removed\n", procfs_name);
}

module_init(procfs1_init);
module_exit(procfs1_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("RC");
MODULE_DESCRIPTION("Generates primes using Eratosthenes sieve.");