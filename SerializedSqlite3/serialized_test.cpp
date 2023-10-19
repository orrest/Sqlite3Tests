#include "gtest/gtest.h"
#include "pch.h"
#include "sqlite3.h"

#include <iostream>
#include <thread>
#include <vector>

namespace {

    TEST(SERIALIZED_TEST, THREAD_MODE) {
        std::cout << sqlite3_threadsafe() << std::endl;
    }

    TEST(SERIALIZED_TEST, SELECT) {
        sqlite3* db = 0;
        sqlite3_stmt* stmt = 0;
        int retcode;

        retcode = sqlite3_open("MyDB", &db);
        if (retcode != SQLITE_OK) {
            sqlite3_close(db);
            fprintf(stderr, "Could not open the MyDB database\n");
            printf("Return code: %d", retcode);
        }

        retcode = sqlite3_prepare(db, "select COUNT(SID) from Students", -1, &stmt, 0);
        if (retcode != SQLITE_OK) {
            sqlite3_close(db);
            fprintf(stderr, "Could not compile a select statement\n");
            printf("Return code: %d", retcode);
        }
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int i = sqlite3_column_int(stmt, 0);
            printf("COUNT = %d\n", i);
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        printf("Return code: %d", SQLITE_OK);
    }

    /// <summary>
    /// 多个线程各自使用自己的数据库连接时会发生 SQLITE_BUSY == 5 的问题.
    /// sqlite有自己的数据库连接池吗?
    /// </summary>
    void InsertIsolatedDbHandle() {
        sqlite3* db = 0;
        sqlite3_stmt* stmt = 0;

        int retcode = sqlite3_open("MyDB", &db);
        if (retcode != SQLITE_OK) {
            sqlite3_close(db);
            fprintf(stderr, "Could not open the MyDB database\n");
            printf("Return code: %d\n", retcode);
        }

        retcode = sqlite3_prepare(db, "insert into Students values (100);", -1, &stmt, 0);
        if (retcode != SQLITE_OK) {
            sqlite3_close(db);
            fprintf(stderr, "Could not compile a select statement\n");
            printf("Return code: %d\n", retcode);
        }

        retcode = sqlite3_step(stmt);
        if (retcode == SQLITE_DONE) {
            std::cout << std::this_thread::get_id() << " INSERTED!" << std::endl;
        }
        else {
            // SQLITE_BUSY == 5
            std::cout << std::this_thread::get_id() << " return code "  << 
                retcode << std::endl;
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    void InsertSingleDbHandle(sqlite3* db) {

        sqlite3_stmt* stmt = 0;

        int retcode = sqlite3_prepare(db, "insert into Students values (100);", -1, &stmt, 0);
        if (retcode != SQLITE_OK) {
            sqlite3_close(db);
            fprintf(stderr, "Could not compile a select statement\n");
            printf("Return code: %d\n", retcode);
        }

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            std::cout << std::this_thread::get_id() << " INSERTED!" << std::endl;
        }

        sqlite3_finalize(stmt);
    }

    TEST(SERIALIZED_TEST, ISOLATED_DB_HANDLE) {
        std::vector<std::thread> threads;
        int numThreads = 5;

        // 创建线程并将其加入容器
        for (int i = 0; i < numThreads; ++i)
        {
            threads.push_back(std::thread(InsertIsolatedDbHandle));
        }

        // 等待所有线程完成
        for (auto& thread : threads)
        {
            thread.join();
        }

        std::cout << "All threads have completed." << std::endl;
    }

    TEST(SERIALIZED_TEST, SINGLE_DB_HANDLE) {
        std::vector<std::thread> threads;
        int numThreads = 5;

        // 创建数据库连接，多个线程共享一个数据库连接
        sqlite3* db = 0;
        int retcode = sqlite3_open("MyDB", &db);
        if (retcode != SQLITE_OK) {
            sqlite3_close(db);
            fprintf(stderr, "Could not open the MyDB database\n");
            printf("Return code: %d\n", retcode);
        }

        // 创建线程并将其加入容器
        for (int i = 0; i < numThreads; ++i)
        {
            threads.push_back(std::thread(InsertSingleDbHandle, db));
        }

        // 等待所有线程完成
        for (auto& thread : threads)
        {
            thread.join();
        }

        std::cout << "All threads have completed." << std::endl;

        // 关闭数据库连接
        sqlite3_close(db);
    }

    static int callback(void *uuused, int argc, char **argv, char** colName) {
        for (int i = 0; i < argc; i++) {
            std::cout << colName[i] << " = " << (argv[i] ? argv[i] : "NULL") << '\t';
        }
        std::cout << std::endl;
        return 0;
    }

    TEST(SERIALIZED_TEST, LIB_CONN_DB_CONN) {
        sqlite3* db = 0;
        sqlite3_open("MyDB", &db);
        sqlite3_exec(db, "attach database MyDBExtn as DB1", 0, 0, 0);
        sqlite3_exec(db, "delete from Students where SID=100;", 0, 0, 0);
        int retcode = sqlite3_exec(db, "select * from Students S, Courses C where S.sid = C.sid", callback, 0, 0);
        if (retcode != SQLITE_OK) {
            std::cout << retcode << sqlite3_errmsg(db) << std::endl;
        }
        sqlite3_close(db);
    }

    TEST(SERIALIZED_TEST, TRANSACTION) {
        sqlite3* db = 0;
        sqlite3_open("MyDB", &db);
        sqlite3_exec(db, "attach database MyDBExtn as DB1", 0, 0, 0);
        
        sqlite3_exec(db, "begin", 0, 0, 0);
        sqlite3_exec(db, "delete from Students where SID = 2000", 0, 0, 0);
        sqlite3_exec(db, "delete from Courses where SID = 2000", 0, 0, 0);

        // see journal
        std::this_thread::sleep_for(std::chrono::seconds(5));

        sqlite3_exec(db, "insert into Students values (2000)", 0, 0, 0);
        sqlite3_exec(db, "insert into Courses values ('SQLite Database', 2000)", 0, 0, 0);
        sqlite3_exec(db, "commit", 0, 0, 0);
        sqlite3_close(db);
    }

    void TransactionExceptionThrow() {
        sqlite3* db = 0;
        sqlite3_open("MyDB", &db);
        sqlite3_exec(db, "attach database MyDBExtn as DB1", 0, 0, 0);

        sqlite3_exec(db, "begin", 0, 0, 0);
        sqlite3_exec(db, "delete from Students where SID = 2000", 0, 0, 0);
        sqlite3_exec(db, "delete from Courses where SID = 2000", 0, 0, 0);

        throw std::runtime_error("Intend error thrown to see the journal log.");

        sqlite3_exec(db, "insert into Students values (2000)", 0, 0, 0);
        sqlite3_exec(db, "insert into Courses values ('SQLite Database', 2000)", 0, 0, 0);
        sqlite3_exec(db, "commit", 0, 0, 0);
        sqlite3_close(db);
    }

    TEST(SERIALIZED_TEST, TRANSACTION_JOURNAL) {
        ASSERT_ANY_THROW(TransactionExceptionThrow());
    }
}
