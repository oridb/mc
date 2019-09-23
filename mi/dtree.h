typedef struct Dtree Dtree;
struct Dtree {
	int id;
	Srcloc loc;

	/* values for matching */
	Node *lbl;
	Node *load;
	size_t nconstructors;
	char accept;
	char emitted;
	char ptrwalk;

	/* the decision tree step */
	Node **pat;
	size_t npat;
	Dtree **next;
	size_t nnext;
	Dtree *any;

	/* captured variables and action */
	Node **cap;
	size_t ncap;
};


