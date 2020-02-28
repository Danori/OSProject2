#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#define PAGE_FAULT -1

struct PageTableEntry
{
    int pageNum;
    bool dirty;
    bool secondChance;
};

struct PageTable
{
    struct PageTableEntry *entries;
    int numEntries;
    bool isFull;
};

struct Node
{
    int index;
    struct Node *prev;
    struct Node *next;
};

struct DLinkedList
{
    int numNodes;
    struct Node *header;
    struct Node *trailer;
};

// Page Table functions.
struct PageTable initPageTable();
void printPageTable(struct PageTable pageTable);
unsigned int findEntry(struct PageTable pageTable, unsigned int pageNum);

// Linked List functions.
struct DLinkedList initLinkedList();
void addNode(struct DLinkedList *list, int index);
void removeNode(struct DLinkedList *list, int index);
void insertAtFront(struct DLinkedList *list, struct Node *node);
struct Node *findNode(struct DLinkedList *list, int index);
void updateRecency(struct DLinkedList *list, int index);
void freeList(struct DLinkedList *list);
void printList(struct DLinkedList *list);

// Random helper functions.
unsigned int getPageNum(unsigned int address);

// Replacement Policy functions.
void rdm();
void lru();
void fifo();
void vms();

/**
 * Global variables just to avoid passing them / returning them
 * to / from the replacement policy functions.
 */
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

struct DLinkedList initLinkedList()
{
    printf("Adding header and trailer nodes ...\n");
    struct DLinkedList list;
    
    list.header = malloc(sizeof(struct Node));
    list.trailer = malloc(sizeof(struct Node));

    list.numNodes = 0;

    list.header->index = -1;
    list.header->next = list.trailer;
    list.header->prev = NULL;

    list.trailer->index = -1;
    list.trailer->prev = list.header;
    list.trailer->next = NULL;

    return list;
}

void addNode(struct DLinkedList *list, int index)
{
    printf("Adding node %d at front ...\n", index);
    struct Node *newNode = malloc(sizeof(struct Node));

    newNode->index = index;

    newNode->prev = list->header;
    newNode->next = list->header->next;
    list->header->next->prev = newNode;
    list->header->next = newNode;

    list->numNodes++;
}

void removeNode(struct DLinkedList *list, int index)
{
    struct Node *node = findNode(list, index);

    if (node != NULL) {
        printf("Removing node %d ...\n", index);
        node->prev->next = node->next;
        node->next->prev = node->prev;

        free(node);

        list->numNodes--;
    }

}

void insertAtFront(struct DLinkedList *list, struct Node *node)
{
    printf("Inserting node %d at front ...\n", node->index);

    struct Node *prev = node->prev;
    struct Node *next = node->next;

    prev->next = next;
    next->prev = prev;
    node->prev = list->header;
    node->next = list->header->next;
    list->header->next->prev = node;
    list->header->next = node;
}

struct Node *findNode(struct DLinkedList *list, int index)
{
    struct Node *currNode = list->header;

    while (currNode->next != list->trailer) {
        currNode = currNode->next;

        if (currNode->index == index) {
            printf("Found node %d ...\n", index);
            return currNode;
        }
    }

    printf("Unable to find node %d ...\n", index);
    return NULL;
}

void freeList(struct DLinkedList *list)
{
    while (list->numNodes != 0) {
        removeNode(list, list->header->next->index);
    }

    printf("Removing header and trailer sentinel nodes ...\n");
    free(list->header);
    free(list->trailer);
    printf("Entire list successfully freed ...\n");
}

unsigned int getPageNum(unsigned int address)
{
    return (address & 0xFFFFF000) >> 12;
}

unsigned int findEntry(struct PageTable pageTable, unsigned int pageNum)
{
    for (int i = 0; i < numFrames; i++) {
        if (pageTable.entries[i].pageNum == pageNum) {
            return i;
        }
    }

    return PAGE_FAULT;
}

void printDebugInfo(struct PageTable pageTable)
{
    printf("NumReads: %-8d NumWrites: %-8d\n\n", numReads, numWrites);

    printf("PAGE TABLE\n");
    printf("numEntries: %-6d isFull: %d\n", pageTable.numEntries, pageTable.isFull);
    printf("============================\n");
    printf("Entry: PageNumber:    Dirty:\n");
    for (int i = 0; i < numFrames; i++) {
        printf("%-6d 0x%08x     %d\n", i, pageTable.entries[i].pageNum, pageTable.entries[i].dirty);
    }
}

void printList(struct DLinkedList *list)
{
    printf("Printing list on following line ...\n");
    struct Node *currNode = list->header;

    while (currNode->next != list->trailer) {
        currNode = currNode->next;
        printf("%d ", currNode->index);
    }
    printf("\n");
}

void rdm()
{
    srand(time(0));

    struct PageTable pageTable = initPageTable();

    unsigned int address, pageNum;
    char rw, exitCh;

    int index = 0, randIndex = 0;
    while (fscanf(traceFile, "%x %c", &address, &rw) != EOF) {
        numEvents++;
        pageNum = getPageNum(address);

        if (debug) {
            printf("Next Address: 0x%08x RW: %c PageNumber: 0x%08x\n", address, rw, pageNum);
            printDebugInfo(pageTable);
            printf("Enter x to exit. ");

            exitCh = getchar();
            if (exitCh == 'X' || exitCh == 'x') {
                free(pageTable.entries);
                exit(0);
            }
        }
        
        index = findEntry(pageTable, pageNum);

        if (index != PAGE_FAULT) {
            if (rw == 'R') {
                continue;
            }
            else {
                pageTable.entries[index].dirty = true;
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
    struct DLinkedList list = initLinkedList();

    addNode(&list, 0);
    addNode(&list, 5);
    addNode(&list, 10);

    printList(&list);

    addNode(&list, 15);
    addNode(&list, 20);

    struct Node *node = findNode(&list, 5);
    if (node != NULL) {
        insertAtFront(&list, node);
    }

    node = findNode(&list, 5);
    if (node != NULL) {
        insertAtFront(&list, node);
    }

    node = findNode(&list, 0);
    if (node != NULL) {
        insertAtFront(&list, node);
    }

    printList(&list);

    removeNode(&list, 10);
    removeNode(&list, 35);

    printList(&list);

    freeList(&list);
}

void fifo()
{

}

void vms()
{

}
