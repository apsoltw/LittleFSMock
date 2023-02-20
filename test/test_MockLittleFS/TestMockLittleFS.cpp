#include <sys/stat.h>

#include <unity.h>

#include "LittleFS.h"

#define TEST_DIR ".unittest/"
#define BASE_NAME "unit_test"
#define FOLDER_NAME "unit_test/folder"
#define FILE_NAME "unit_test/file.txt"
#define MAKE_FILE_NAME(var_name, file_name) \
    char var_name[strlen(BASE_NAME)+1+strlen(file_name)+1];\
    strcpy(var_name,BASE_NAME);\
    strcat(var_name, "/");\
    strcat(var_name, file_name);

void rawCreateFile(const char* content = nullptr, const char* name = FILE_NAME)
{
    String s = TEST_DIR;
    s += (name[0] == '/' ? name+1 : name);
    FILE* fp = fopen(s.c_str(), "w");
    if ( content != nullptr )
        fprintf(fp, "%s", content);
    fclose(fp);
}

void rawCreateFolder(const char* name = FOLDER_NAME)
{
    String s = TEST_DIR;
    s += (name[0] == '/' ? name+1 : name);
    mkdir(s.c_str());
}

bool rawDetectFile(const char* name = FILE_NAME)
{
    String s = TEST_DIR;
    s += (name[0] == '/' ? name+1 : name);
    struct stat stats;
    stat(s.c_str(), &stats);
    return S_ISREG(stats.st_mode) ? true : false;
}

bool rawDetectFolder(const char* name = FOLDER_NAME)
{
    String s = TEST_DIR;
    s += (name[0] == '/' ? name+1 : name);
    struct stat stats;
    stat(s.c_str(), &stats);
    return S_ISDIR(stats.st_mode) ? true : false;
}

bool rawRemoveFile(const char* name = FILE_NAME)
{
    String s = TEST_DIR;
    s += (name[0] == '/' ? name+1 : name);
    return remove(s.c_str()) == 0;
}

bool rawRemoveFolder(const char* name = FOLDER_NAME)
{
    String s = TEST_DIR;
    s += (name[0] == '/' ? name+1 : name);
    return rmdir(s.c_str()) == 0;
}

size_t rawReadFile(void* pBuf, size_t count, const char* name = FILE_NAME)
{
    String s = TEST_DIR;
    s += (name[0] == '/' ? name+1 : name);
    int readCount;
    FILE* fp;
    fp = fopen(s.c_str(), "r");
    size_t countRead = fread(pBuf, 1, count, fp);
    fclose(fp);
    return countRead;
}

void testFsIsMounted(void)
{
    FSInfo info;
    TEST_ASSERT_TRUE(LittleFS.info(info));
}

void testFsExists(void)
{
    TEST_ASSERT_FALSE(LittleFS.exists(FILE_NAME));

    rawCreateFile();
    TEST_ASSERT_TRUE(LittleFS.exists(FILE_NAME));
}

void testFsRename(void)
{
    MAKE_FILE_NAME(targetName, "file2.txt");

    TEST_ASSERT_FALSE(LittleFS.rename(FILE_NAME, targetName));

    rawCreateFile();
    TEST_ASSERT_TRUE(LittleFS.rename(FILE_NAME, targetName));
    TEST_ASSERT_TRUE(rawDetectFile(targetName));
    rawRemoveFile(targetName);
}

void testFsCreateFolder(void)
{
    LittleFS.mkdir(FOLDER_NAME);
    TEST_ASSERT_TRUE(rawDetectFolder());
}

void testFsRemoveFolder(void)
{
    TEST_ASSERT_FALSE(LittleFS.rmdir(FOLDER_NAME));
    rawCreateFolder();
    TEST_ASSERT_TRUE(LittleFS.rmdir(FOLDER_NAME));
    TEST_ASSERT_FALSE(rawDetectFolder());
}


void testFsCreateFile(void)
{
    File file = LittleFS.open(FILE_NAME, "w");
    TEST_ASSERT_TRUE(file.isFile());
    file.close();
    TEST_ASSERT_TRUE(rawDetectFile());
}

void testFsRemoveFile(void)
{
    TEST_ASSERT_FALSE(LittleFS.remove(FILE_NAME));
    rawCreateFile();
    TEST_ASSERT_TRUE(LittleFS.remove(FILE_NAME));
    TEST_ASSERT_FALSE(rawDetectFile());
}

void testFileRead(void)
{
    uint8_t buf[11];
    memset(buf, 0, sizeof(buf));
    char content[] = "This is the content of the file.";

    rawCreateFile(content);
    File file = LittleFS.open(FILE_NAME, "r");
    size_t count = file.read(buf, 10);
    file.close();
    TEST_ASSERT_EQUAL_size_t(10, count);
    TEST_ASSERT_EQUAL_CHAR_ARRAY(content, buf, 10);
}

void testFileWrite(void)
{
    size_t count;
    char buf[50];
    memset(buf, 0, sizeof(buf));
    char contentWrite[] = "The content to write.";
    char contentAppend[] = "The content to append.";

    rawCreateFile();

    File file = LittleFS.open(FILE_NAME, "w");
    file.write((uint8_t*) contentWrite, strlen(contentWrite));
    file.close();

    count = rawReadFile(buf, 25);
    TEST_ASSERT_EQUAL_size_t(strlen(contentWrite), count);
    TEST_ASSERT_EQUAL_CHAR_ARRAY(contentWrite, buf, strlen(contentWrite));

    file = LittleFS.open(FILE_NAME, "a");
    file.write((uint8_t*) contentAppend, strlen(contentAppend));
    file.close();

    count = rawReadFile(buf, 50);
    TEST_ASSERT_EQUAL_size_t(strlen(contentWrite) + strlen(contentAppend), count);
    TEST_ASSERT_EQUAL_CHAR_ARRAY(contentWrite, buf, strlen(contentWrite));
    TEST_ASSERT_EQUAL_CHAR_ARRAY(contentAppend, buf+strlen(contentWrite), strlen(contentAppend));
}

void testFileSeek(void)
{
    char content[] = "0123456789";
    rawCreateFile(content);

    File file = LittleFS.open(FILE_NAME, "r");

    file.seek(3);
    TEST_ASSERT_EQUAL_CHAR('3', (char)file.read());

    file.seek(4, SeekMode::SeekCur);
    TEST_ASSERT_EQUAL_CHAR('8', (char)file.read());

    file.seek(3, SeekMode::SeekEnd);
    TEST_ASSERT_EQUAL_CHAR('7', (char)file.read());

    file.seek(-3, SeekMode::SeekCur);
    TEST_ASSERT_EQUAL_CHAR('5', (char)file.read());

    file.close();
}

void testFilePosition(void)
{
    char content[] = "0123456789";
    rawCreateFile(content);

    File file = LittleFS.open(FILE_NAME, "r");
    file.read();
    file.read();
    TEST_ASSERT_EQUAL_size_t(2, file.position());

    file.close();
}

void testFileTruncate(void)
{
    char content[] = "01234567890123456789";
    rawCreateFile(content);

    File file = LittleFS.open(FILE_NAME, "r");
    TEST_ASSERT_EQUAL_size_t(20, file.size());

    bool res = file.truncate(10);
    TEST_ASSERT_FALSE(res);
    file.close();

    file = LittleFS.open(FILE_NAME, "a+");
    TEST_ASSERT_EQUAL_size_t(20, file.size());

    res = file.truncate(10);
    TEST_ASSERT_TRUE(res);
    TEST_ASSERT_EQUAL_size_t(10, file.size());

    file.close();

    char buf[25];
    size_t newSize = rawReadFile(buf, 25);
    TEST_ASSERT_EQUAL_size_t(10, newSize);
    TEST_ASSERT_EQUAL_CHAR_ARRAY("0123456789", buf, 10);
}

void testFileName(void)
{
    rawCreateFile();
    File file = LittleFS.open(FILE_NAME, "r");
    TEST_ASSERT_EQUAL_STRING("file.txt", file.name());
    file.close();
}

void testFileFullName(void)
{
    rawCreateFile();
    File file = LittleFS.open(FILE_NAME, "r");
    String s = TEST_DIR;
    s += FILE_NAME;
    TEST_ASSERT_EQUAL_STRING(s.c_str(), file.fullName());
    file.close();
}

void testFileIsFile(void)
{
    File file;

    file = LittleFS.open(FILE_NAME, "r");
    TEST_ASSERT_FALSE(file.isFile());
    file.close();

    rawCreateFile();
    file = LittleFS.open(FILE_NAME, "r");
    TEST_ASSERT_TRUE(file.isFile());
    file.close();
}

void testFileIsDirectory(void)
{
    rawCreateFile();
    File file = LittleFS.open(FILE_NAME, "r");
    TEST_ASSERT_FALSE(file.isDirectory());
    file.close();
}

void testDirBrowse(void)
{
    MAKE_FILE_NAME(file1, "file1.txt");
    rawCreateFile("0123456789");
    rawCreateFolder();
    rawCreateFile("0123456789", file1);

    String s;
    Dir dir = LittleFS.openDir(BASE_NAME);
    uint8_t counter = 0;
    while(dir.next())
    {
        counter++;
        String s = dir.fileName();
        const char* name = s.c_str();
        TEST_ASSERT_UNLESS((strcmp("file.txt", name) && strcmp("file1.txt", name) && strcmp("folder", name)));

        if (strcmp("folder", name) == 0)
        {
            TEST_ASSERT_TRUE(dir.isDirectory());
            TEST_ASSERT_FALSE(dir.isFile());
            TEST_ASSERT_EQUAL_size_t(0, dir.fileSize());
        }
        else
        {
            TEST_ASSERT_FALSE(dir.isDirectory());
            TEST_ASSERT_TRUE(dir.isFile());
            TEST_ASSERT_EQUAL_size_t(10, dir.fileSize());
        }
    }
    TEST_ASSERT_EQUAL_UINT8(3, counter);
    rawRemoveFile(file1);
}

void testDirRewind(void)
{
    rawCreateFile();
    rawCreateFolder();

    Dir dir = LittleFS.openDir(BASE_NAME);
    TEST_ASSERT_TRUE(dir.next());
    TEST_ASSERT_TRUE(dir.next());
    TEST_ASSERT_FALSE(dir.next());

    bool res = dir.rewind();
    TEST_ASSERT_TRUE(dir.next());
    TEST_ASSERT_TRUE(dir.next());
    TEST_ASSERT_FALSE(dir.next());
}

void testDirOpenFile(void)
{
    rawCreateFolder();
    rawCreateFile("123");

    Dir dir = LittleFS.openDir(BASE_NAME);
    bool found = false;
    while (dir.next())
    {
        String s = dir.fileName();
        if (s.equals("file.txt"))
        {
            found = true;
            File file = dir.openFile("r");
            TEST_ASSERT_TRUE(file.isFile());
            TEST_ASSERT_EQUAL_INT('1', file.read());
            TEST_ASSERT_EQUAL_INT('2', file.read());
            TEST_ASSERT_EQUAL_INT('3', file.read());
            TEST_ASSERT_EQUAL_INT(-1, file.read());
            file.close();
        }
    }
    TEST_ASSERT_TRUE(found);
}

void testAllInRoot(void)
{
    rawCreateFile("ABC", "fileInRoot.txt");
    File file = LittleFS.open("/fileInRoot.txt", "r");
    TEST_ASSERT_TRUE(file.isFile());
    file.close();

    Dir dir = LittleFS.openDir("/");
    TEST_ASSERT_TRUE(dir.next());
    String s = dir.fileName();
    TEST_ASSERT_EQUAL_STRING("fileInRoot.txt", s.c_str());
    file = dir.openFile("r");
    TEST_ASSERT_TRUE(file.isFile());
    TEST_ASSERT_EQUAL_INT('A', file.read());
    file.close();
    TEST_ASSERT_TRUE(LittleFS.rename("/fileInRoot.txt", "fileToRemove.txt"));
    TEST_ASSERT_FALSE(rawDetectFile("fileInRoot.txt"));
    TEST_ASSERT_TRUE(LittleFS.remove("/fileToRemove.txt"));
    TEST_ASSERT_FALSE(rawDetectFile("fileToRemove.txt"));
}

void setUp(void)
{
    mkdir(TEST_DIR);
    rawCreateFolder(BASE_NAME);
}

void tearDown(void)
{
    rawRemoveFile(FILE_NAME);
    rawRemoveFolder(FOLDER_NAME);
    if (!rawRemoveFolder(BASE_NAME) && errno != ENOENT)
        // something prevents removing the test folder
        TEST_FAIL();
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    LittleFS.begin(argc, argv);

    RUN_TEST(testFsIsMounted);
    RUN_TEST(testFsExists);
    RUN_TEST(testFsRename);
    RUN_TEST(testFsCreateFolder);
    RUN_TEST(testFsRemoveFolder);
    RUN_TEST(testFsCreateFile);
    RUN_TEST(testFsRemoveFile);
    RUN_TEST(testFileRead);
    RUN_TEST(testFileWrite);
    RUN_TEST(testFileSeek);
    RUN_TEST(testFilePosition);
    RUN_TEST(testFileTruncate);
    RUN_TEST(testFileName);
    RUN_TEST(testFileFullName);
    RUN_TEST(testFileIsFile);
    RUN_TEST(testFileIsDirectory);
    RUN_TEST(testDirBrowse);
    RUN_TEST(testDirRewind);
    RUN_TEST(testDirOpenFile);
    RUN_TEST(testAllInRoot);

    LittleFS.end();
    UNITY_END();
}
