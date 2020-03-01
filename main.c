#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#define PAGE_FAULT -1
#define PROCESS_B 0x30000000

struct PageTableEntry
{
    int pageNum;
    bool dirty;
};

struct PageTable
{
    struct PageTableEntry *entries;
    int numEntries;
    bool isFull;
};

struct Node
{
    struct PageTableEntry *page;
    struct Node *prev;
    struct Node *next;
};

struct DLinkedList
{
    int numNodes;
    struct Node *header;
    struct Node *trailer;
};

struct PageTable initPageTable();
void printPageTable(struct PageTable pageTable);
struct PageTableEntry *findEntry(struct PageTable pageTable, unsigned int pageNum);

struct DLinkedList initLinkedList();
void addFront(struct DLinkedList *list, struct PageTableEntry *page);
void rmFront(struct DLinkedList *list);
void insertFront(struct DLinkedList *list, struct Node *node);
void addBack(struct DLinkedList *list, struct PageTableEntry *page);
void rmBack(struct DLinkedList *list);
void insertBack(struct DLinkedList *list, struct Node *node);
struct Node *findNode(struct DLinkedList *list, struct PageTableEntry *page);
void rmNode(struct DLinkedList *list, struct PageTableEntry *page);
void updateRecency(struct DLinkedList *list, struct PageTableEntry *page);
struct PageTableEntry *getLeastRecent(struct DLinkedList *list);
void freeList(struct DLinkedList *list);
void printList(struct DLinkedList *list);

unsigned int getPageNum(unsigned int address);
unsigned int getProcess(unsigned int address);

void rdm();
void lru();
void fifo();
void vms();

FILE *traceFile;
int numFrames = 0, numReads = 0, 
numWrites = 0, numEvents = 0;
char *replacementPolicy;
bool debug;

int main(int argc, char *argv[])
{
    if (argc != 5) {
        printf("Usage: memsim <tracefile> <numframes> <rdm|lru|fifo|vms> "
        "<debug|quiet>\n");
        return -1;
    }

    traceFile = fopen(argv[1], "r");
    if (traceFile == NULL) {
        printf("Failed to open %s. Ensure proper file name and file is in " 
        "proper directory and try again.\n", argv[1]);
        return -1;
    }

    sscanf(argv[2], "%d", &numFrames);
    replacementPolicy = argv[3];
    char *debugStr = argv[4];
    if (strcmp(debugStr, "debug") == 0) {
        debug = true;
    }
    else {
        debug = false;
    }
    if (strcmp(replacementPolicy, "rdm") == 0) {
        rdm();
    }
    else if (strcmp(replacementPolicy, "lru") == 0) {
        lru();
    }
    else if (strcmp(replacementPolicy, "fifo") == 0) {
        fifo();
    }
    else if (strcmp(replacementPolicy, "vms") == 0) {
        vms();
    }
    else {
        printf("Unrecognized replacement policy. Options: rdm lru fifo vms\n");
    }

    printf("Total memory frames: %d\n", numFrames);
    printf("Events in trace: %d\n", numEvents);
    printf("Total disk reads: %d\n", numReads);
    printf("Total disk writes: %d\n", numWrites);

    return 0;
}

struct PageTable initPageTable()
{
    struct PageTable pageTable;
    pageTable.entries = malloc(numFrames * sizeof(struct PageTableEntry));

    for (int i = 0; i < numFrames; i++) {
        pageTable.entries[i].pageNum = -1;
        pageTable.entries[i].dirty = false;
    }

    pageTable.numEntries = 0;
    pageTable.isFull = false;

    return pageTable;
}

void printPageTable(struct PageTable pageTable)
{
    printf("PAGE TABLE\n");
    printf("numEntries: %-6d isFull: %d\n", pageTable.numEntries, pageTable.isFull);
    printf("============================\n");
    printf("Entry: PageNumber:    Dirty:\n");
    for (int i = 0; i < numFrames; i++) {
        printf("%-6d 0x%08x     %d\n", i, pageTable.entries[i].pageNum, pageTable.entries[i].dirty);
    }
    printf("============================\n");
}

struct PageTableEntry *findEntry(struct PageTable pageTable, unsigned int pageNum)
{
    for (int i = 0; i < numFrames; i++) {
        if (pageTable.entries[i].pageNum == pageNum) {
            return &pageTable.entries[i];
        }
    }

    return NULL;
}

struct DLinkedList initLinkedList()
{
    
    struct DLinkedList list;
    
    list.header = malloc(sizeof(struct Node));
    list.trailer = malloc(sizeof(struct Node));

    list.numNodes = 0;

    list.header->page = NULL;
    list.header->next = list.trailer;
    list.header->prev = NULL;

    list.trailer->page = NULL;
    list.trailer->prev = list.header;
    list.trailer->next = NULL;

    return list;
}


void addFront(struct DLinkedList *list, struct PageTableEntry *page)
{
    
    struct Node *newNode = malloc(sizeof(struct Node));

    newNode->page = page;

    newNode->prev = list->header;
    newNode->next = list->header->next;
    list->header->next->prev = newNode;
    list->header->next = newNode;

    list->numNodes++;
}

void rmFront(struct DLinkedList *list)
{
    struct Node *node = list->header->next;

    if (node != list->trailer) {
        
        node->prev->next = node->next;
        node->next->prev = node->prev;

        free(node);

        list->numNodes--;
    }
}


void insertFront(struct DLinkedList *list, struct Node *node)
{
    struct Node *prev = node->prev;
    struct Node *next = node->next;

    prev->next = next;
    next->prev = prev;
    node->prev = list->header;
    node->next = list->header->next;
    list->header->next->prev = node;
    list->header->next = node;
}

void addBack(struct DLinkedList *list, struct PageTableEntry *page)
{
    struct Node *newNode = malloc(sizeof(struct Node));

    newNode->page = page;

    newNode->next = list->trailer;
    newNode->prev = list->trailer->prev;
    list->trailer->prev->next = newNode;
    list->trailer->prev = newNode;

    list->numNodes++;
}

void rmBack(struct DLinkedList *list)
{
    struct Node *node = list->trailer->prev;

    if (node != list->header) {
        node->prev->next = node->next;
        node->next->prev = node->prev;

        free(node);

        list->numNodes--;
    }
}


void insertBack(struct DLinkedList *list, struct Node *node)
{
    struct Node *prev = node->prev;
    struct Node *next = node->next;

    prev->next = next;
    next->prev = prev;
    node->next = list->trailer;
    node->prev = list->trailer->prev;
    list->trailer->prev->next = node;
    list->trailer->prev = node;
}


struct Node *findNode(struct DLinkedList *list, struct PageTableEntry *page)
{
    struct Node *currNode = list->header;

    while (currNode->next != list->trailer) {
        currNode = currNode->next;

        if (currNode->page == page) {
            
            return currNode;
        }
    }
    
    return NULL;
}


void rmNode(struct DLinkedList *list, struct PageTableEntry *page)
{
    struct Node *node = findNode(list, page);

    if (node != NULL) {
        node->prev->next = node->next;
        node->next->prev = node->prev;

        free(node);

        list->numNodes--;
    }

}

void updateRecency(struct DLinkedList *list, struct PageTableEntry *page)
{
    struct Node *node = findNode(list, page);

    if (node != NULL) {
        insertFront(list, node);
    }
}


struct PageTableEntry *getLeastRecent(struct DLinkedList *list)
{
    return list->trailer->prev->page;
}

void freeList(struct DLinkedList *list)
{
    while (list->numNodes != 0) {
        rmFront(list);
    }
    
    free(list->header);
    free(list->trailer);
    
}


unsigned int getPageNum(unsigned int address)
{
    return address >> 12;
}

unsigned int getProcess(unsigned int address)
{
    return address & 0xF0000000;
}


void printList(struct DLinkedList *list)
{
    if (list->numNodes == 0) {
        printf("{EMPTY}");
    }

    struct Node *currNode = list->header;

    while (currNode->next != list->trailer) {
        currNode = currNode->next;
        printf("0x%08x ", currNode->page->pageNum);
    }
    printf("\n");
}


void rdm()
{
    srand(time(0));
    
    struct PageTable pageTable = initPageTable();

    unsigned int address, pageNum;
    char rw, exitCh;

    struct PageTableEntry *page;
    int randIndex = 0;
    
    while (fscanf(traceFile, "%x %c", &address, &rw) != EOF) {
        numEvents++;
        pageNum = getPageNum(address);

        if (debug) {
            printf("NumReads: %-8d NumWrites: %-8d\n\n", numReads, numWrites);
            printPageTable(pageTable);
            printf("NxtPN: 0x%08x RW: %c \n", pageNum, rw);
            printf("Enter x to exit. ");

            exitCh = getchar();
            if (exitCh == 'X' || exitCh == 'x') {
                free(pageTable.entries);
                exit(0);
            }
        }
        
        page = findEntry(pageTable, pageNum);

        if (page != NULL) {
            if (rw == 'W') {
                page->dirty = true;
            }
        }
        
        else {
            if (!pageTable.isFull) {
                pageTable.entries[pageTable.numEntries].pageNum = pageNum;

                if (rw == 'W') {
                    pageTable.entries[pageTable.numEntries].dirty = true;
                }

                pageTable.numEntries++;
                numReads++;

                if (pageTable.numEntries == numFrames) {
                    pageTable.isFull = true;
                }
            }
            else {
                randIndex = rand() % numFrames;
                
                if (pageTable.entries[randIndex].dirty) {
                    numWrites++;
                }

                pageTable.entries[randIndex].pageNum = pageNum;
                numReads++;
                
                if (rw == 'R') {
                    pageTable.entries[randIndex].dirty = false;
                }
                else {
                    pageTable.entries[randIndex].dirty = true;
                }
            }
        }
    }

    free(pageTable.entries);
}

void lru()
{
    struct PageTable pageTable = initPageTable();
    struct DLinkedList recencyList = initLinkedList();

    unsigned int address, pageNum;
    char rw, exitCh;

    struct PageTableEntry *page, *pageToRemove;
    while (fscanf(traceFile, "%x %c", &address, &rw) != EOF) {
        numEvents++;
        pageNum = getPageNum(address);

        if (debug) {
            printf("NumReads: %-8d NumWrites: %-8d\n\n", numReads, numWrites);
            printPageTable(pageTable);
            printf("RECENCY LIST\n");
            printList(&recencyList);
            printf("NxtPN: 0x%08x RW: %c\n", pageNum, rw);
            printf("Enter x to exit. ");

            exitCh = getchar();
            if (exitCh == 'X' || exitCh == 'x') {
                free(pageTable.entries);
                freeList(&recencyList);
                exit(0);
            }
        }
        
        page = findEntry(pageTable, pageNum);

        if (page != NULL) {
            updateRecency(&recencyList, page);

            if (rw == 'W') {
                page->dirty = true;
            }
        }
        else {
            if (!pageTable.isFull) {
                pageTable.entries[pageTable.numEntries].pageNum = pageNum;

                if (rw == 'W') {
                    pageTable.entries[pageTable.numEntries].dirty = true;
                }

                addFront(&recencyList, &pageTable.entries[pageTable.numEntries]);

                pageTable.numEntries++;
                numReads++;

                if (pageTable.numEntries == numFrames) {
                    pageTable.isFull = true;
                }
            }
            else {
                pageToRemove = getLeastRecent(&recencyList);
                
                if (pageToRemove->dirty) {
                    numWrites++;
                }

                pageToRemove->pageNum = pageNum;
                numReads++;
                if (rw == 'W') {
                    pageToRemove->dirty = true;
                }
                else {
                    pageToRemove->dirty = false;
                }

                updateRecency(&recencyList, pageToRemove);
            }
        }
    }

    free(pageTable.entries);
    freeList(&recencyList);
}

void fifo()
{

}

void vms()
{
    struct PageTable pageTable = initPageTable();
    struct DLinkedList afifo = initLinkedList();
    struct DLinkedList bfifo = initLinkedList();
    struct DLinkedList clean = initLinkedList();
    struct DLinkedList dirty = initLinkedList();

    unsigned int address, pageNum, process, rss = numFrames / 2;
    char rw, exitCh;
    bool isProcessA;

    struct PageTableEntry *page, *pageToRemove;
    while (fscanf(traceFile, "%x %c", &address, &rw) != EOF) {
        numEvents++;
        pageNum = getPageNum(address);
        process = getProcess(address);
        isProcessA = process != PROCESS_B ? true : false;

        if (debug) {
            printf("NumReads: %-8d NumWrites: %-8d\n\n", numReads, numWrites);
            printPageTable(pageTable);
            printf("A FIFO\n");
            printList(&afifo);
            printf("B FIFO\n");
            printList(&bfifo);
            printf("CLEAN LIST\n");
            printList(&clean);
            printf("DIRTY LIST\n");
            printList(&dirty);
            printf("NxtPN: 0x%08x RW: %c\n", pageNum, rw);
            printf("Enter x to exit. ");

            exitCh = getchar();
            if (exitCh == 'X' || exitCh == 'x') {
                free(pageTable.entries);
                freeList(&afifo);
                freeList(&bfifo);
                freeList(&clean);
                freeList(&dirty);
                exit(0);
            }
        }
        
        page = findEntry(pageTable, pageNum);

        struct Node *node;
        if (page != NULL) {
            node = findNode(&dirty, page);
            if (node != NULL) {
                rmNode(&dirty, page);
            }

            node = findNode(&clean, page);
            if (node != NULL) {
                rmNode(&clean, page);
            }

            if (isProcessA) {
                node = findNode(&afifo, page);

                if (node != NULL) {
                    if (rw == 'W') {
                        node->page->dirty = true;
                    }
                }
                else {
                    addFront(&afifo, page);

                    if (afifo.numNodes >= rss) {
                        pageToRemove = getLeastRecent(&afifo);
                        rmNode(&afifo, pageToRemove);

                        if (pageToRemove->dirty) {
                            addFront(&dirty, pageToRemove);
                        }
                        else {
                            addFront(&clean, pageToRemove);
                        }

                    }
                }
            }
            else {
                node = findNode(&bfifo, page);

                if (node != NULL) {
                    if (rw == 'W') {
                        node->page->dirty = true;
                    }
                }
                else {
                    addFront(&bfifo, page);

                    if (bfifo.numNodes >= rss) {
                        pageToRemove = getLeastRecent(&bfifo);
                        rmNode(&bfifo, pageToRemove);

                        if (pageToRemove->dirty) {
                            addFront(&dirty, pageToRemove);
                        }
                        else {
                            addFront(&clean, pageToRemove);
                        }

                    }
                }
            }
        }
        else {
            if (!pageTable.isFull) {
                pageTable.entries[pageTable.numEntries].pageNum = pageNum;

                if (rw == 'W') {
                    pageTable.entries[pageTable.numEntries].dirty = true;
                }

                if (isProcessA) {
                    if (afifo.numNodes >= rss) {
                        pageToRemove = getLeastRecent(&afifo);
                        rmNode(&afifo, pageToRemove);

                        if (pageToRemove->dirty) {
                            addFront(&dirty, pageToRemove);
                        }
                        else {
                            addFront(&clean, pageToRemove);
                        }
                    }
                    addFront(&afifo, &pageTable.entries[pageTable.numEntries]);
                }
                else {
                    if (bfifo.numNodes >= rss) {
                        pageToRemove = getLeastRecent(&bfifo);
                        rmNode(&bfifo, pageToRemove);

                        if (pageToRemove->dirty) {
                            addFront(&dirty, pageToRemove);
                        }
                        else {
                            addFront(&clean, pageToRemove);
                        }
                    }
                    addFront(&bfifo, &pageTable.entries[pageTable.numEntries]);
                }

                pageTable.numEntries++;
                numReads++;

                if (pageTable.numEntries == numFrames) {
                    pageTable.isFull = true;
                }
            }
            else {
                if (isProcessA) {
                    if (clean.numNodes > 0) {
                        pageToRemove = getLeastRecent(&clean);
                        rmNode(&clean, pageToRemove);
                    } 
                    else if (dirty.numNodes > 0) {
                        pageToRemove = getLeastRecent(&dirty);
                        rmNode(&dirty, pageToRemove); 
                    }
                    else {
                        pageToRemove = getLeastRecent(&afifo);
                        rmNode(&afifo, pageToRemove);
                    }

                    addFront(&afifo, pageToRemove);

                    if (afifo.numNodes > rss) {
                        page = getLeastRecent(&afifo);
                        rmBack(&afifo);

                        if (page->dirty) {
                            addFront(&dirty, page);
                        }
                        else {
                            addFront(&clean, page);
                        }
                    }
                }
                else {
                    if (clean.numNodes > 0) {
                        pageToRemove = getLeastRecent(&clean);
                        rmNode(&clean, pageToRemove);
                    }
                    else if (dirty.numNodes > 0) {
                        pageToRemove = getLeastRecent(&dirty);
                        rmNode(&dirty, pageToRemove);
                    }
                    else {
                        pageToRemove = getLeastRecent(&bfifo);
                        rmNode(&bfifo, pageToRemove);
                    }

                    addFront(&bfifo, pageToRemove);

                    if (bfifo.numNodes > rss) {
                        page = getLeastRecent(&bfifo);
                        rmBack(&bfifo);

                        if (page->dirty) {
                            addFront(&dirty, page);
                        }
                        else {
                            addFront(&clean, page);
                        }
                    }
                }

                if (pageToRemove->dirty) {
                    numWrites++;
                }

                pageToRemove->pageNum = pageNum;
                numReads++;
                if (rw == 'W') {
                    pageToRemove->dirty = true;
                }
                else {
                    pageToRemove->dirty = false;
                }
            }
        }
    }

    free(pageTable.entries);
    freeList(&afifo);
    freeList(&bfifo);
    freeList(&clean);
    freeList(&dirty);
}
