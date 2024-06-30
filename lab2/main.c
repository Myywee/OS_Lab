#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void print(const char *str);

int bytesPerSec = 512; // 每扇区字节数
int secPerClus = 1;    // 每簇的扇区数
int rsvdSecCnt = 1;    // Boot记录占用的扇区数
int numFATS = 2;       // FAT表个数
int rootEntCnt = 224;  // 根目录最大文件数
int FATSize = 9;       // FAT扇区数
const char *errorOP = "error: Please use the correct command\n";
const char *errorDir = "error: This dir doesn't exsist\n";
const char *errorFile = "error: This file doesn't exsist\n";
const char *errorPath = "error: This path is repeated\n";

#pragma pack(push)
#pragma pack(1) // 构造体中的成员会紧密排列，没有间隙

typedef struct entry
{
    char dirName[11];          // 文件名
    unsigned char dirAttr;     // 文件属性
    char reserve[10];          // 保留位
    unsigned short dirWrtTime; // 最后一次写入时间
    unsigned short dirWrtDate; // 最后一次写入日期
    unsigned short dirFstClus; // 文件开始的簇号
    unsigned int dirFileSize;  // 文件大小
} Entry;

typedef struct node
{
    char *name;    // 文件或者目录的名称
    int type;      // 表示节点的类型 '1' 表示目录 '2' 表示文件
    int fileSize;  // 表示文件大小
    int dirCount;  // 记录下一级目录数量
    int fileCount; // 记录下一级文件数量
    char *content; // 记录文件内容
    struct node *next[100];
} Node;

struct BPB
{
    unsigned short BPB_BytsPerSec; // 每扇区字节数
    unsigned char BPB_SecPerClus;  // 每簇扇区数
    unsigned short BPB_RsvdSecCnt; // Boot记录占用的扇区数
    unsigned char BPB_NumFATs;     // FAT表个数
    unsigned short BPB_RootEntCnt; // 根目录最大文件数
    unsigned short BPB_TotSec16;   // 扇区总数
    unsigned char BPB_Media;       // 介质描述符
    unsigned short BPB_FATSz16;    // FAT扇区数
    unsigned short BPB_SecPerTrk;  //	每磁道扇区数
    unsigned short BPB_NumHeads;   // 磁头数（面数）
    unsigned int BPB_HiddSec;      // 隐藏扇区数
    unsigned int BPB_TotSec32;     // 如果BPB_FATSz16为0，该值为FAT扇区数
};

#pragma pack(pop) // 恢复为默认的对齐方式

void readRoot(FILE *fat12, Entry *entry_ptr, Node *root); // 读取根目录
int checkChar(char word);
void getContent(FILE *fat12, int dirFstClus, int fileSize, Node *node);
int getNextClus(FILE *fat12, int clus);
void getDir(FILE *fat12, int dirFstClus, Node *node);
char *handlePath(char *path);
Node *findFile(Node *node, char *filePath);
void printLs(Node *node, char *path);
void printLsL(Node *node, char *path);

Node *root;

int main()
{
    FILE *fat12;
    fat12 = fopen("lab2.img", "rb");

    root = (Node *)malloc(sizeof(Node));
    root->name = "";

    Entry rootEntry;
    Entry *rootEntry_ptr = &rootEntry;
    readRoot(fat12, rootEntry_ptr, root);

    while (1)
    {

        print("> ");
        char inputString[100];
        memset(inputString, '\0', sizeof(inputString));
        fgets(inputString, sizeof(inputString), stdin);
        int size = 0;
        int index = 0;
        char input[100][100];
        memset(input, '\0', sizeof(input));
        int length = strlen(inputString);
        for (int i = 0; i < length; i++)
        {
            if (inputString[i] == ' ')
            {
                size++;
                index = 0;
            }
            else if (inputString[i] == '\n')
            {
                continue;
            }
            else
            {
                input[size][index++] = inputString[i];
            }
        }

        size++;
        if (strlen(input[0]) == 0)
        {
            print(errorOP);
            continue;
        }
        if (strncmp(input[0], "exit", strlen(input[0])) == 0)
        {
            fclose(fat12);
            return 0;
        }
        else if (strncmp(input[0], "ls", strlen(input[0])) == 0)
        {
            if (size == 1)
            {
                printLs(root, "/");
            }
            else
            {
                int hasL = 0;
                int hasPath = 0;
                int error = 0;
                char *path = "/";
                for (int i = 1; i < size; i++)
                {
                    if (input[i][0] != '-')
                    {
                        if (hasPath)
                        {
                            print(errorPath);
                            error = 1;
                            break;
                        }
                        else
                        {
                            path = handlePath(input[i]);
                            hasPath = 1;
                        }
                    }
                    else
                    {
                        if (strlen(input[i]) == 1)
                        {
                            print(errorOP);
                            error = 1;
                            break;
                        }

                        for (int j = 1; j < strlen(input[i]); j++)
                        {
                            if (input[i][j] != 'l')
                            {
                                print(errorOP);
                                error = 1;
                                break;
                            }
                        }
                        hasL = 1;
                    }
                }

                if (error == 1)
                {
                    continue;
                }

                Node *temp = findFile(root, path);
                if (temp == NULL)
                {
                    print(errorDir);
                    continue;
                }
                if (hasL)
                    printLsL(temp, path);
                else
                    printLs(temp, path);
            }
        }
        else if (strncmp(input[0], "cat", strlen(input[0])) == 0)
        {
            if (size == 2 && input[1][0] != '-')
            {
                char *filePath = handlePath(input[1]);
                Node *ans = findFile(root, filePath);
                if (ans == NULL)
                {
                    print(errorFile);
                    continue;
                }
                else if (ans->type == 1)
                {
                    print(errorFile);
                    continue;
                }
                else
                {
                    char *content = ans->content;
                    print(content);
                    print("\n");
                }
            }
            else
            {
                print(errorOP);
                continue;
            }
        }
        else
        {
            print(errorOP);
            continue;
        }
    }

    return 0;
}

void readRoot(FILE *fat12, Entry *entry_ptr, Node *root)
{
    int offset = (rsvdSecCnt + numFATS * FATSize) * bytesPerSec; // 根目录首字节偏移量
    int check;
    char name[12];
    memset(name, '\0', sizeof(name));

    // 依次处理根目录的各个条目
    for (int i = 0; i < rootEntCnt; i++)
    {
        check = fseek(fat12, offset, SEEK_SET);
        if (check == -1)
        {
            print("error");
        }

        check = fread(entry_ptr, 1, 32, fat12); // 读取一个条目
        if (check != 32)
        {
            print("error");
        }

        offset += 32;

        int flag = 0;
        for (int j = 0; j < 11; j++)
        {
            if (checkChar(entry_ptr->dirName[j]))
            {
                flag = 1;
                break;
            }
        }

        if (flag == 1)
            continue;

        if (entry_ptr->dirAttr == 32)
        {
            // 文件
            int index = -1;
            for (int k = 0; k < 11;)
            {
                if (entry_ptr->dirName[k] != ' ')
                {
                    name[++index] = entry_ptr->dirName[k++];
                }
                else
                {
                    name[++index] = '.';
                    while (entry_ptr->dirName[k] == ' ')
                        k++;
                }
            }
            name[++index] = '\0'; // 读取名字操作结束
            Node *temp = (Node *)malloc(sizeof(Node));
            temp->name = malloc(11 * sizeof(char));
            strcpy(temp->name, name);
            temp->fileSize = entry_ptr->dirFileSize;
            temp->type = 2; // 文件类型
            root->next[root->fileCount + root->dirCount] = temp;
            root->fileCount++;
            temp->content = malloc(entry_ptr->dirFileSize);
            getContent(fat12, entry_ptr->dirFstClus, entry_ptr->dirFileSize, temp);
        }
        else
        {
            // 目录
            int index = -1;
            for (int k = 0; k < 11; k++)
            {
                if (entry_ptr->dirName[k] != ' ')
                {
                    name[++index] = entry_ptr->dirName[k];
                }
                else
                {
                    name[++index] = '\0';
                    break;
                }
            }
            Node *temp = (Node *)malloc(sizeof(Node));
            temp->name = malloc(11 * sizeof(char));
            strcpy(temp->name, name);
            temp->type = 1;
            root->next[root->fileCount + root->dirCount] = temp;
            root->dirCount++;
            getDir(fat12, entry_ptr->dirFstClus, temp);
        }
    }
}

int checkChar(char word)
{
    if (((word >= 48) && (word <= 57)) || ((word >= 65) && (word <= 90)) || ((word >= 97) && (word <= 122)) || (word == ' '))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

void getContent(FILE *fat12, int dirFstClus, int fileSize, Node *node)
{
    // 偏移量 =  （引导扇区数 + FAT表扇区数 + 根目录扇区数（取整）） * 每个扇区字节数
    int offset = bytesPerSec * (rsvdSecCnt + FATSize * numFATS + (rootEntCnt * 32 + bytesPerSec - 1) / bytesPerSec);
    int clus = dirFstClus;
    if (clus == 0 || clus == 1)
        return;

    int nextClus = 0;

    int size = secPerClus * bytesPerSec;
    int index = 0;
    int maxSize = fileSize;
    while (nextClus < 0xFF8)
    {
        nextClus = getNextClus(fat12, clus);
        if (nextClus == 0xFF7)
        {
            print("error");
        }

        char *content = (char *)malloc(size);

        int startBytes = offset + (clus - 2) * secPerClus * bytesPerSec;
        int check = 0;
        check = fseek(fat12, startBytes, SEEK_SET);
        if (check == -1)
        {
            print("error");
        }
        check = fread(content, 1, size, fat12);
        if (check != size)
        {
            print("error");
        }

        for (int i = 0; i < size; i++)
        {
            if (index >= fileSize)
                break;
            else
            {
                node->content[index++] = content[i];
            }
        }

        free(content);
        clus = nextClus;
    }
}

int getNextClus(FILE *fat12, int clus)
{

    // 一组中第2字节的低半字节作为最高半字节和一组中第1字节组成整数表示一个簇号，第2字节的高半字节作为最低半字节和第3字节组成整数表示一个簇号
    int offset = rsvdSecCnt * bytesPerSec + clus * 3 / 2;

    // 先读出fat所在的两个字节
    unsigned short bytes;
    unsigned short *bytes_ptr = &bytes;
    int check;

    check = fseek(fat12, offset, SEEK_SET);
    if (check == -1)
    {
        print("error");
    }

    check = fread(bytes_ptr, 1, 2, fat12);
    if (check != 2)
    {
        print("error");
    }

    if (clus % 2 != 0)
    {
        return bytes >> 4;
    }
    else
    {
        bytes = bytes << 4;
        return bytes >> 4;
    }
}

void getDir(FILE *fat12, int dirFstClus, Node *node)
{
    // 偏移量
    int offset = bytesPerSec * (rsvdSecCnt + FATSize * numFATS + (rootEntCnt * 32 + bytesPerSec - 1) / bytesPerSec);
    int clus = dirFstClus;
    if (clus == 0 || clus == 1)
        return;

    int nextClus = 0;
    while (nextClus < 0xFF8)
    {
        nextClus = getNextClus(fat12, clus);
        if (nextClus == 0xFF7)
        {
            print("error");
        }

        int size = secPerClus * bytesPerSec;
        int startBytes = offset + (clus - 2) * size;
        int check = 0;
        int count = 0;

        while (count < size)
        {
            Entry entry;
            Entry *entry_ptr = &entry;

            check = fseek(fat12, startBytes + count, SEEK_SET);
            if (check == -1)
            {
                print("error");
            }

            check = fread(entry_ptr, 1, 32, fat12);
            if (check != 32)
            {
                print("error");
            }

            count += 32;
            if (entry_ptr->dirName[0] == '\0')
                continue;

            // 根据条目内容进行判断
            int flag = 0;
            for (int j = 0; j < 11; j++)
            {
                if (checkChar(entry_ptr->dirName[j]))
                {
                    flag = 1;
                    break;
                }
            }

            if (flag == 1)
                continue; // 如果名字不符合要求不输出

            char name[12];
            // 处理名字
            if (entry_ptr->dirAttr == 32)
            {
                // 文件
                int index = -1;
                for (int k = 0; k < 11;)
                {
                    if (entry_ptr->dirName[k] != ' ')
                    {
                        name[++index] = entry_ptr->dirName[k++];
                    }
                    else
                    {
                        name[++index] = '.';
                        while (entry_ptr->dirName[k] == ' ')
                            k++;
                    }
                }
                name[++index] = '\0'; // 读取名字操作结束
                Node *temp = (Node *)malloc(sizeof(Node));
                temp->name = malloc(11 * sizeof(char));
                strcpy(temp->name, name);
                temp->fileSize = entry_ptr->dirFileSize;
                temp->type = 2; // 文件类型
                node->next[node->fileCount + node->dirCount] = temp;
                node->fileCount++;
                temp->content = malloc(entry_ptr->dirFileSize);
                getContent(fat12, entry_ptr->dirFstClus, entry_ptr->dirFileSize, temp);
            }
            else
            {
                // 目录
                int index = -1;
                for (int k = 0; k < 11; k++)
                {
                    if (entry_ptr->dirName[k] != ' ')
                    {
                        name[++index] = entry_ptr->dirName[k];
                    }
                    else
                    {
                        name[++index] = '\0';
                        break;
                    }
                }
                Node *temp = (Node *)malloc(sizeof(Node));
                temp->type = 1;
                temp->name = malloc(11 * sizeof(char));
                strcpy(temp->name, name);
                node->next[node->fileCount + node->dirCount] = temp;
                node->dirCount++;
                getDir(fat12, entry_ptr->dirFstClus, temp);
            }
        }
    }
}

char *handlePath(char *path)
{
    int length = strlen(path);
    int size = -1;
    int index = 0;
    char input[100][100];
    memset(input, '\0', 10000);
    for (int i = 0; i < length; i++)
    {
        if (path[i] == '/' && i != length - 1)
        {
            size++;
            index = 0;
        }
        else
        {
            if (size == -1)
                size = 0;
            input[size][index++] = path[i];
        }
    }
    size++;
    char *ans = malloc(size + 1 + strlen(path));
    int num = 0;
    *(ans + num++) = '/';
    for (int i = 0; i < size; i++)
    {
        if (strncmp(input[i], "..", 2) == 0)
        {
            num--;
            while (num > 0)
            {
                *(ans + num) = '\0';
                num--;
                if (*(ans + num) == '/')
                {
                    break;
                }
            }
        }
        else
        {
            for (int j = 0; j < strlen(input[i]); j++)
            {
                *(ans + num++) = input[i][j];
            }
            *(ans + num++) = '/';
            Node *temp = findFile(root, ans);
            if (temp == NULL)
            {
                return "error";
            }
        }
    }

    return ans;
}

Node *findFile(Node *node, char *filePath)
{
    int len = strlen(filePath);
    if ((len == 1) && (strncmp(filePath, "/", 1) == 0))
    {
        return node;
    }
    char input[2][100];
    memset(input[0], '\0', 100);
    memset(input[1], '\0', 100);
    int indexPath = 0;
    int index = 0;
    if (filePath[0] == '/')
        indexPath++;

    while (indexPath < len)
    {
        if (filePath[indexPath] == '/')
        {
            index = 0;
            break;
        }
        else
        {
            input[0][index++] = filePath[indexPath++];
        }
    }

    for (int i = indexPath; i < len; i++)
    {
        input[1][index++] = filePath[i];
    }

    int size = node->dirCount + node->fileCount;
    for (int i = 0; i < size; i++)
    {
        Node *temp = node->next[i];
        if (strncmp(temp->name, input[0], sizeof(temp->name)) == 0)
        {
            return findFile(temp, input[1]);
        }
    }

    return NULL;
}

void printLs(Node *node, char *path)
{
    if (node->type == 2)
    {
        return;
    }

    print(path);
    print(":\n");
    print("\033[31m");
    print(". .. ");
    print("\033[0m");

    int size = node->dirCount + node->fileCount;

    for (int i = 0; i < size; i++)
    {
        Node *temp = node->next[i];
        if (temp->type == 1)
        {
            print("\033[31m");
            print(temp->name);
            print("\033[0m");
            print(" ");
        }
        else
        {
            print(temp->name);
            print(" ");
        }
    }

    print("\n");

    for (int i = 0; i < size; i++)
    {
        Node *temp = node->next[i];
        char *nextPath = malloc(strlen(path) + strlen(temp->name) + 1);
        memset(nextPath, '\0', strlen(path) + strlen(temp->name) + 1);
        int index = 0;
        for (int j = 0; j < strlen(path); j++)
        {
            *(nextPath + index++) = path[j];
        }
        for (int j = 0; j < strlen(temp->name); j++)
        {
            *(nextPath + index++) = temp->name[j];
        }
        *(nextPath + index) = '/';
        printLs(temp, nextPath);
    }
}

void printLsL(Node *node, char *path)
{
    if (node->type == 2)
    {
        return;
    }

    char temp1[20];
    memset(temp1, '\0', 20);
    sprintf(temp1, "%d", node->dirCount);

    char temp2[20];
    memset(temp2, '\0', 20);
    sprintf(temp2, "%d", node->fileCount);

    print(path);
    print(" ");
    print(temp1);
    print(" ");
    print(temp2);
    print(":\n");
    print("\033[31m");
    print(".\n..\n");
    print("\033[0m");
    int size = node->dirCount + node->fileCount;

    for (int i = 0; i < size; i++)
    {
        Node *temp = node->next[i];
        if (temp->type == 1)
        {
            char temp3[20];
            memset(temp3, '\0', 20);
            sprintf(temp3, "%d", temp->dirCount);

            char temp4[20];
            memset(temp4, '\0', 20);
            sprintf(temp4, "%d", temp->fileCount);

            print("\033[31m");
            print(temp->name);
            print("\033[0m");
            print(" ");
            print(temp3);
            print(" ");
            print(temp4);
            print("\n");
        }
        else
        {
            char temp3[20];
            memset(temp3, '\0', 20);
            sprintf(temp3, "%d", temp->fileSize);

            print(temp->name);
            print(" ");
            print(temp3);
            print("\n");
        }
    }

    print("\n");

    for (int i = 0; i < size; i++)
    {
        Node *temp = node->next[i];
        char *nextPath = malloc(strlen(path) + strlen(temp->name) + 1);
        int index = 0;
        for (int j = 0; j < strlen(path); j++)
        {
            *(nextPath + index++) = path[j];
        }
        for (int j = 0; j < strlen(temp->name); j++)
        {
            *(nextPath + index++) = temp->name[j];
        }
        *(nextPath + index) = '/';
        printLsL(temp, nextPath);
    }
}
