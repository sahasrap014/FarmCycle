/*
 * FarmCycle (Simple Version) - Agricultural Waste Marketplace
 * Compile: gcc farmcycle_simple.c -o farmcycle
 *
 * DSA used:
 *  Linked List    - farmers, listings, manufacturers, orders
 *  Graph          - materials <-> products (adjacency matrix)
 *  BFS / DFS      - raw materials for a product / products from a material
 *  BST            - listings by price (recursive inorder)
 *  Priority Queue - purchase requests (max-heap, biggest order first)
 *  Stack          - undo
 *  Heap Sort      - suppliers by price
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_NODES 50
#define MAX_PQ 500
#define PENDING 0
#define CONFIRMED 1
#define CANCELLED 2

/* ---------- structures ---------- */
typedef struct Farmer {
    int num; char name[50], village[50], phone[11];
    struct Farmer *next;
} Farmer;

typedef struct Listing {
    int num, farmerNum, qty; char material[50]; double price;
    struct Listing *next;
} Listing;

typedef struct Order {
    int num, mfrNum, qty, status; double total;
    Listing *listing;
    struct Order *next;
} Order;

typedef struct StackNode { Order *order; struct StackNode *next; } StackNode;

typedef struct Manufacturer {
    int num; char name[50], city[50];
    StackNode *undoTop;
    struct Manufacturer *next;
} Manufacturer;

typedef struct BstNode { Listing *listing; struct BstNode *left, *right; } BstNode;

/* ---------- globals ---------- */
Farmer *farmerHead = NULL;
Listing *listingHead = NULL;
Manufacturer *mfrHead = NULL;
Order *orderHead = NULL;
BstNode *priceRoot = NULL;

Order *pq[MAX_PQ];                    /* priority queue (max-heap) */
int pqSize = 0;

char names[MAX_NODES][50];            /* graph node names */
int isProduct[MAX_NODES];             /* 0 = material, 1 = product */
int connected[MAX_NODES][MAX_NODES];  /* adjacency matrix */
int nodeCount = 0;

int nextFarmer = 101, nextListing = 101, nextMfr = 101, nextOrder = 101;
const char *predefined[4] = {"Rice Straw", "Coconut Husk", "Sugarcane Bagasse", "Groundnut Shells"};

/* ---------- input helpers ---------- */
void readLine(char *buf, int size) {
    if (fgets(buf, size, stdin) == NULL) { printf("\nBye!\n"); exit(0); }
    buf[strcspn(buf, "\r\n")] = '\0';
    int len = strlen(buf);
    while (len > 0 && buf[len - 1] == ' ') buf[--len] = '\0';
}

int readInt(const char *prompt, int min, int max) {
    char buf[50];
    while (1) {
        printf("%s", prompt);
        readLine(buf, sizeof(buf));
        if (isdigit((unsigned char)buf[0]) && atoi(buf) >= min && atoi(buf) <= max)
            return atoi(buf);
        printf("  Enter a number from %d to %d.\n", min, max);
    }
}

double readPrice(const char *prompt) {
    char buf[50];
    while (1) {
        printf("%s", prompt);
        readLine(buf, sizeof(buf));
        if (atof(buf) > 0) return atof(buf);
        printf("  Enter a positive price.\n");
    }
}

void readText(const char *prompt, char *buf, int size) {
    do {
        printf("%s", prompt);
        readLine(buf, size);
    } while (buf[0] == '\0');
}

int validPhone(const char *p) {              /* 10 digits, starts with 6-9 */
    int i;
    if (strlen(p) != 10 || p[0] < '6' || p[0] > '9') return 0;
    for (i = 0; i < 10; i++)
        if (!isdigit((unsigned char)p[i])) return 0;
    return 1;
}

int sameText(const char *a, const char *b) { /* ignore upper/lower case */
    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
    return *a == '\0' && *b == '\0';
}

int askId(const char *prompt) {              /* "F101" or "101" -> 101 */
    char buf[50];
    printf("%s", prompt);
    readLine(buf, sizeof(buf));
    return isalpha((unsigned char)buf[0]) ? atoi(buf + 1) : atoi(buf);
}

/* ---------- linked list searches ---------- */
Farmer *findFarmer(int num) {
    Farmer *f;
    for (f = farmerHead; f != NULL; f = f->next)
        if (f->num == num) return f;
    return NULL;
}

Listing *findListing(int num) {
    Listing *l;
    for (l = listingHead; l != NULL; l = l->next)
        if (l->num == num) return l;
    return NULL;
}

Manufacturer *findManufacturer(int num) {
    Manufacturer *m;
    for (m = mfrHead; m != NULL; m = m->next)
        if (m->num == num) return m;
    return NULL;
}

/* ---------- graph ---------- */
int findNode(const char *name, int type) {
    int i;
    for (i = 0; i < nodeCount; i++)
        if (isProduct[i] == type && sameText(names[i], name)) return i;
    return -1;
}

int addNode(const char *name, int type) {
    int i = findNode(name, type);
    if (i != -1) return i;
    if (nodeCount >= MAX_NODES) { printf("Graph is full!\n"); return -1; }
    strcpy(names[nodeCount], name);
    isProduct[nodeCount] = type;
    return nodeCount++;
}

void addLink(int a, int b) { connected[a][b] = 1; connected[b][a] = 1; }

void buildGraph(void) {
    int rice = addNode("Rice Straw", 0), bag = addNode("Sugarcane Bagasse", 0);
    int husk = addNode("Coconut Husk", 0), shell = addNode("Groundnut Shells", 0);
    int paper = addNode("Paper", 1), pack = addNode("Packaging Material", 1);
    int compost = addNode("Compost", 1), fiber = addNode("Coir Fiber", 1);
    int rope = addNode("Coir Rope", 1), mats = addNode("Coir Mats", 1);
    addLink(rice, paper); addLink(rice, pack); addLink(rice, compost);
    addLink(bag, paper);  addLink(bag, pack);  addLink(bag, compost);
    addLink(husk, fiber); addLink(husk, rope); addLink(husk, mats);
    addLink(shell, compost);
}

void printNames(int type) {
    int i;
    printf("   ");
    for (i = 0; i < nodeCount; i++)
        if (isProduct[i] == type) printf("%s, ", names[i]);
    printf("\n");
}

int dfsCount = 0;
void dfs(int node, int depth, int visited[]) {   /* recursive DFS from a material */
    int i;
    visited[node] = 1;
    if (depth == 0) printf("[Material] %s\n", names[node]);
    else { printf("   -> [Product] %s\n", names[node]); dfsCount++; }
    if (isProduct[node]) return;
    for (i = 0; i < nodeCount; i++)
        if (connected[node][i] && !visited[i]) dfs(i, depth + 1, visited);
}

void bfs(int start, int level[]) {               /* level[i] = distance from start */
    int queue[MAX_NODES], front = 0, rear = 0, i, cur;
    for (i = 0; i < nodeCount; i++) level[i] = -1;
    level[start] = 0;
    queue[rear++] = start;
    while (front < rear) {
        cur = queue[front++];
        for (i = 0; i < nodeCount; i++)
            if (connected[cur][i] && level[i] == -1) {
                level[i] = level[cur] + 1;
                queue[rear++] = i;
            }
    }
}

/* ---------- BST (by price) ---------- */
BstNode *bstInsert(BstNode *root, Listing *l) {
    if (root == NULL) {
        root = (BstNode *)malloc(sizeof(BstNode));
        root->listing = l;
        root->left = root->right = NULL;
    } else if (l->price < root->listing->price) root->left = bstInsert(root->left, l);
    else root->right = bstInsert(root->right, l);
    return root;
}

int shown = 0;
void bstInorder(BstNode *root) {                 /* lowest price first */
    if (root == NULL) return;
    bstInorder(root->left);
    if (root->listing->qty > 0) {
        printf("  L%-6d F%-6d %-20s %-8d Rs %.2f\n", root->listing->num,
               root->listing->farmerNum, root->listing->material,
               root->listing->qty, root->listing->price);
        shown++;
    }
    bstInorder(root->right);
}

/* ---------- priority queue (max-heap by order total) ---------- */
int higher(Order *a, Order *b) {                 /* bigger total first; tie -> older order first */
    return a->total > b->total || (a->total == b->total && a->num < b->num);
}

void enqueue(Order *o) {                         /* insert at end, move UP */
    int i = pqSize++;
    pq[i] = o;
    while (i > 0 && higher(pq[i], pq[(i - 1) / 2])) {
        Order *t = pq[i]; pq[i] = pq[(i - 1) / 2]; pq[(i - 1) / 2] = t;
        i = (i - 1) / 2;
    }
}

Order *dequeue(void) {                           /* take root, move last to root, move DOWN */
    Order *top, *t;
    int i = 0, best, l, r;
    if (pqSize == 0) return NULL;
    top = pq[0];
    pq[0] = pq[--pqSize];
    while (1) {
        l = 2 * i + 1; r = 2 * i + 2; best = i;
        if (l < pqSize && higher(pq[l], pq[best])) best = l;
        if (r < pqSize && higher(pq[r], pq[best])) best = r;
        if (best == i) break;
        t = pq[i]; pq[i] = pq[best]; pq[best] = t;
        i = best;
    }
    return top;
}

/* ---------- stack (undo) ---------- */
void push(Manufacturer *m, Order *o) {
    StackNode *n = (StackNode *)malloc(sizeof(StackNode));
    n->order = o; n->next = m->undoTop;
    m->undoTop = n;
}

Order *pop(Manufacturer *m) {
    StackNode *n = m->undoTop;
    Order *o;
    if (n == NULL) return NULL;
    o = n->order;
    m->undoTop = n->next;
    free(n);
    return o;
}

/* ---------- heap sort (by price, lowest first) ---------- */
void heapify(Listing *a[], int n, int i) {       /* push a[i] down so the max-heap rule holds */
    int big = i, l = 2 * i + 1, r = 2 * i + 2;
    Listing *t;
    if (l < n && a[l]->price > a[big]->price) big = l;
    if (r < n && a[r]->price > a[big]->price) big = r;
    if (big != i) {
        t = a[i]; a[i] = a[big]; a[big] = t;
        heapify(a, n, big);
    }
}

void heapSort(Listing *a[], int n) {
    int i;
    Listing *t;
    for (i = n / 2 - 1; i >= 0; i--) heapify(a, n, i);   /* build max-heap */
    for (i = n - 1; i > 0; i--) {                        /* move the max to the end */
        t = a[0]; a[0] = a[i]; a[i] = t;
        heapify(a, i, 0);
    }
}

/* ---------- suppliers: search + sort ---------- */
void showSuppliers(const char *material) {
    Listing *arr[100], *p;
    Farmer *f;
    int count = 0, i;
    for (p = listingHead; p != NULL && count < 100; p = p->next)
        if (p->qty > 0 && sameText(p->material, material)) arr[count++] = p;
    if (count == 0) { printf("  No farmer has '%s' available now.\n", material); return; }
    heapSort(arr, count);
    printf("  Suppliers of %s (cheapest first):\n", material);
    for (i = 0; i < count; i++) {
        f = findFarmer(arr[i]->farmerNum);
        printf("  L%d | F%d %s, %s, %s | %d kg | Rs %.2f/kg\n", arr[i]->num,
               f->num, f->name, f->village, f->phone, arr[i]->qty, arr[i]->price);
    }
    f = findFarmer(arr[0]->farmerNum);
    printf("  >> Best price: L%d from %s (F%d) at Rs %.2f/kg\n",
           arr[0]->num, f->name, f->num, arr[0]->price);
}

/* ---------- farmer portal ---------- */
void registerFarmer(void) {
    Farmer *f = (Farmer *)malloc(sizeof(Farmer)), *p;
    char phone[50];
    readText("Name   : ", f->name, 50);
    readText("Village: ", f->village, 50);
    while (1) {
        readText("Phone (10 digits, starts with 6-9): ", phone, 50);
        if (validPhone(phone)) break;
        printf("  Invalid phone number. Try again.\n");
    }
    strcpy(f->phone, phone);
    f->num = nextFarmer++;
    f->next = NULL;
    if (farmerHead == NULL) farmerHead = f;
    else {
        for (p = farmerHead; p->next != NULL; p = p->next) { }
        p->next = f;
    }
    printf("Registered! Your Farmer ID is F%d\n", f->num);
}

void linkCustomMaterial(const char *material) {
    char product[50];
    int m = addNode(material, 0);
    printf("New material! Existing products:\n");
    printNames(1);
    printf("Enter the products it can make:\n");
    while (1) {
        readText("  Product: ", product, 50);
        addLink(m, addNode(product, 1));
        if (readInt("  Add another product? (1 = yes, 0 = no): ", 0, 1) == 0) break;
    }
}

void addListing(void) {
    Farmer *f = findFarmer(askId("Farmer ID: "));
    Listing *l, *p;
    char material[50];
    int i, choice, node;
    if (f == NULL) { printf("Farmer not found.\n"); return; }
    for (i = 0; i < 4; i++) printf("%d. %s\n", i + 1, predefined[i]);
    printf("5. Other material\n");
    choice = readInt("Choose material: ", 1, 5);
    if (choice <= 4) strcpy(material, predefined[choice - 1]);
    else {
        readText("Material name: ", material, 50);
        node = findNode(material, 0);
        if (node != -1) strcpy(material, names[node]);   /* already known */
        else linkCustomMaterial(material);
    }
    l = (Listing *)malloc(sizeof(Listing));
    strcpy(l->material, material);
    l->qty = readInt("Quantity (kg): ", 1, 1000000);
    l->price = readPrice("Price per kg (Rs): ");
    l->num = nextListing++;
    l->farmerNum = f->num;
    l->next = NULL;
    if (listingHead == NULL) listingHead = l;
    else {
        for (p = listingHead; p->next != NULL; p = p->next) { }
        p->next = l;
    }
    priceRoot = bstInsert(priceRoot, l);
    printf("Listing created! Listing ID is L%d\n", l->num);
}

void viewMyListings(void) {
    int fnum = askId("Farmer ID: "), found = 0;
    Listing *l;
    if (findFarmer(fnum) == NULL) { printf("Farmer not found.\n"); return; }
    for (l = listingHead; l != NULL; l = l->next)
        if (l->farmerNum == fnum) {
            printf("  L%d | %s | %d kg | Rs %.2f/kg\n", l->num, l->material, l->qty, l->price);
            found = 1;
        }
    if (!found) printf("  No listings yet.\n");
}

void updateQuantity(void) {
    int fnum = askId("Farmer ID: ");
    Listing *l;
    if (findFarmer(fnum) == NULL) { printf("Farmer not found.\n"); return; }
    l = findListing(askId("Listing ID: "));
    if (l == NULL || l->farmerNum != fnum) { printf("Listing not found for you.\n"); return; }
    printf("Current quantity: %d kg\n", l->qty);
    l->qty = readInt("New quantity (kg): ", 0, 1000000);
    printf("Updated.\n");
}

void farmerPortal(void) {
    int c;
    do {
        printf("\n== FARMER PORTAL ==\n1. Register Farmer\n2. Add Crop / Agricultural Waste Listing\n");
        printf("3. View My Listings\n4. Update Available Quantity\n5. Back to Main Menu\n");
        c = readInt("Choice: ", 1, 5);
        if (c == 1) registerFarmer();
        else if (c == 2) addListing();
        else if (c == 3) viewMyListings();
        else if (c == 4) updateQuantity();
    } while (c != 5);
}

/* ---------- manufacturer portal ---------- */
void registerManufacturer(void) {
    Manufacturer *m = (Manufacturer *)malloc(sizeof(Manufacturer)), *p;
    readText("Company name: ", m->name, 50);
    readText("City        : ", m->city, 50);
    m->num = nextMfr++;
    m->undoTop = NULL;
    m->next = NULL;
    if (mfrHead == NULL) mfrHead = m;
    else {
        for (p = mfrHead; p->next != NULL; p = p->next) { }
        p->next = m;
    }
    printf("Registered! Your Manufacturer ID is M%d\n", m->num);
}

void browseListings(void) {
    printf("\nAvailable waste (cheapest first):\n");
    printf("  Listing Farmer  Material             Qty(kg)  Price/kg\n");
    shown = 0;
    bstInorder(priceRoot);
    if (shown == 0) printf("  Nothing available.\n");
}

void purchaseWaste(void) {
    Manufacturer *m = findManufacturer(askId("Manufacturer ID: "));
    Listing *l;
    Farmer *f;
    Order *o, *p;
    int qty;
    if (m == NULL) { printf("Manufacturer not found.\n"); return; }
    if (pqSize >= MAX_PQ) { printf("Too many pending orders. Try later.\n"); return; }
    l = findListing(askId("Listing ID: "));
    if (l == NULL || l->qty <= 0) { printf("Listing not found or sold out.\n"); return; }
    f = findFarmer(l->farmerNum);
    printf("%s from %s (F%d), %s, phone %s\n", l->material, f->name, f->num, f->village, f->phone);
    printf("Available: %d kg at Rs %.2f/kg\n", l->qty, l->price);
    qty = readInt("Quantity needed (kg): ", 1, 1000000);
    if (qty > l->qty) { printf("Only %d kg available.\n", l->qty); return; }

    o = (Order *)malloc(sizeof(Order));
    o->num = nextOrder++;
    o->mfrNum = m->num;
    o->listing = l;
    o->qty = qty;
    o->total = qty * l->price;
    o->status = PENDING;
    o->next = NULL;
    if (orderHead == NULL) orderHead = o;
    else {
        for (p = orderHead; p->next != NULL; p = p->next) { }
        p->next = o;
    }
    l->qty -= qty;                               /* reserve quantity */
    push(m, o);                                  /* so it can be undone */
    enqueue(o);                                  /* goes into the priority queue */
    printf("Order O%d is PENDING. Total Rs %.2f\n", o->num, o->total);
}

int processNextOrder(void) {                     /* highest-value order first */
    Order *o = NULL;
    while (o == NULL && pqSize > 0) {
        o = dequeue();
        if (o->status != PENDING) o = NULL;      /* skip undone orders */
    }
    if (o == NULL) return 0;
    o->status = CONFIRMED;
    printf("  O%d (%s, %d kg, Rs %.2f) -> CONFIRMED\n", o->num, o->listing->material, o->qty, o->total);
    return 1;
}

const char *statusText(int s) {
    return s == PENDING ? "PENDING" : (s == CONFIRMED ? "CONFIRMED" : "CANCELLED");
}

void viewMyOrders(void) {
    int mnum = askId("Manufacturer ID: "), pending = 0, c;
    Order *o;
    if (findManufacturer(mnum) == NULL) { printf("Manufacturer not found.\n"); return; }
    for (o = orderHead; o != NULL; o = o->next) {
        if (o->status == PENDING) pending++;
        if (o->mfrNum == mnum)
            printf("  O%d | F%d | %s | %d kg | Rs %.2f | %s\n", o->num,
                   o->listing->farmerNum, o->listing->material, o->qty, o->total,
                   statusText(o->status));
    }
    if (pending == 0) return;
    printf("%d pending order(s). 0 = skip, 1 = process next, 2 = process all\n", pending);
    c = readInt("Choice: ", 0, 2);
    if (c == 1) processNextOrder();
    else if (c == 2) while (processNextOrder()) { }
}

void undoLastAction(void) {
    Manufacturer *m = findManufacturer(askId("Manufacturer ID: "));
    Order *o;
    if (m == NULL) { printf("Manufacturer not found.\n"); return; }
    while ((o = pop(m)) != NULL) {
        if (o->status == PENDING) {
            o->status = CANCELLED;               /* stays in history */
            o->listing->qty += o->qty;           /* give quantity back */
            printf("Order O%d cancelled, quantity restored.\n", o->num);
            return;
        }
        printf("Order O%d is already confirmed - cannot undo.\n", o->num);
    }
    printf("Nothing to undo.\n");
}

void manufacturerPortal(void) {
    int c;
    do {
        printf("\n== MANUFACTURER PORTAL ==\n1. Register Manufacturer\n2. Browse Available Agricultural Waste\n");
        printf("3. Purchase Agricultural Waste\n4. View My Orders\n5. Undo Last Eligible Action\n6. Back to Main Menu\n");
        c = readInt("Choice: ", 1, 6);
        if (c == 1) registerManufacturer();
        else if (c == 2) browseListings();
        else if (c == 3) purchaseWaste();
        else if (c == 4) viewMyOrders();
        else if (c == 5) undoLastAction();
    } while (c != 6);
}

/* ---------- product & supplier finder ---------- */
void findRawMaterials(void) {                    /* BFS from a product */
    char name[50];
    int level[MAX_NODES], start, i;
    printf("Products:\n");
    printNames(1);
    readText("Product name: ", name, 50);
    start = findNode(name, 1);
    if (start == -1) { printf("Product not found.\n"); return; }
    bfs(start, level);
    for (i = 0; i < nodeCount; i++)
        if (level[i] == 1) { printf("\nRaw material: %s\n", names[i]); showSuppliers(names[i]); }
}

void findProductsFromWaste(void) {               /* DFS from a material */
    char name[50];
    int visited[MAX_NODES] = {0}, start;
    printf("Materials:\n");
    printNames(0);
    readText("Material name: ", name, 50);
    start = findNode(name, 0);
    if (start == -1) { printf("Material not found.\n"); return; }
    dfsCount = 0;
    dfs(start, 0, visited);
    printf("%d product(s) found.\n", dfsCount);
}

void finderPortal(void) {
    int c;
    do {
        printf("\n== PRODUCT & SUPPLIER FINDER ==\n1. Find Raw Materials for a Product\n");
        printf("2. Find Products from Agricultural Waste\n3. Back to Main Menu\n");
        c = readInt("Choice: ", 1, 3);
        if (c == 1) findRawMaterials();
        else if (c == 2) findProductsFromWaste();
    } while (c != 3);
}

/* ---------- main ---------- */
int main(void) {
    int c;
    buildGraph();
    printf("===== FarmCycle - Agricultural Waste Marketplace =====\n");
    do {
        printf("\n== MAIN MENU ==\n1. Farmer Portal\n2. Manufacturer Portal\n");
        printf("3. Product & Supplier Finder\n4. Exit\n");
        c = readInt("Choice: ", 1, 4);
        if (c == 1) farmerPortal();
        else if (c == 2) manufacturerPortal();
        else if (c == 3) finderPortal();
    } while (c != 4);
    printf("Thank you for using FarmCycle!\n");
    return 0;
}