#define NODES_MAX 128
#define NODE_NAME_MAX SHORT_STR

namespace Writer {

	void initGraph();
	void submitNode(const char *name, const char *passage);
	void connectNodes(const char *name1, const char *name2, Dir8 dir);

	void showGraph();

	void js_submitNode(CScriptVar *v, void *userdata);
	void js_connectNodes(CScriptVar *v, void *userdata);

	struct Node {
		char name[NODE_NAME_MAX];
		char passage[PASSAGE_NAME_MAX];
		MintSprite *sprite;

		Node *connectedTo;
		Dir8 connectedDir;
	};

	struct GraphStruct {
		Node nodes[NODES_MAX];
		int nodesNum;
	};

	GraphStruct *graph;

	void initGraph() {
		graph = (GraphStruct *)zalloc(sizeof(GraphStruct));
		jsInterp->addNative("function submitNode(name, passage)", &js_submitNode, 0);
		jsInterp->addNative("function connectNodes(name1, name2, dir)", &js_connectNodes, 0);
	}

	void submitNode(const char *name, const char *passage) {
		Node *node = &graph->nodes[graph->nodesNum++];
		strcpy(node->name, name);
		strcpy(node->passage, passage);
	}

	void showGraph() {
		for (int i = 0; i < graph->nodesNum; i++) {
			Node *node = &graph->nodes[i];

			{ /// Node sprite
				MintSprite *spr = createMintSprite();
				spr->setupRect(128, 128, 0x555555);
				node->sprite = spr;
			}

			{ /// Node text
				MintSprite *spr = createMintSprite();
				spr->setupEmpty(node->sprite->width, node->sprite->height);
				spr->setText(node->name);
				node->sprite->addChild(spr);
				spr->alignOutside(DIR8_CENTER);
			}

		}

		for (int i = 0; i < graph->nodesNum; i++) {
			Node *node = &graph->nodes[i];
			Node *other = node->connectedTo;
			if (other) {
				Dir8 realDir = node->connectedDir;
				if (node->connectedDir == DIR8_LEFT) realDir = DIR8_RIGHT;
				if (node->connectedDir == DIR8_RIGHT) realDir = DIR8_LEFT;

				printf("Connecting %s %s %d(%d)\n", node->name, other->name, node->connectedDir, realDir);
				other->sprite->alignOutside(node->sprite, realDir, 10, 10);
			}
		}
	}

	void connectNodes(const char *name1, const char *name2, Dir8 dir) {
		Node *node1 = NULL;
		Node *node2 = NULL;

		for (int i = 0; i < graph->nodesNum; i++)
			if (streq(graph->nodes[i].name, name1))
				node1 = &graph->nodes[i];

		for (int i = 0; i < graph->nodesNum; i++)
			if (streq(graph->nodes[i].name, name2))
				node2 = &graph->nodes[i];

		Assert(node1);
		Assert(node2);

		node1->connectedTo = node2;
		node1->connectedDir = dir;
	}

	void js_submitNode(CScriptVar *v, void *userdata) {
		const char *name = v->getParameter("name")->getString().c_str();
		const char *passage = v->getParameter("passage")->getString().c_str();
		submitNode(name, passage);
	}

	void js_connectNodes(CScriptVar *v, void *userdata) {
		const char *name1 = v->getParameter("name1")->getString().c_str();
		const char *name2 = v->getParameter("name2")->getString().c_str();
		const char *dir = v->getParameter("dir")->getString().c_str();

		Dir8 newDir;
		if (streq(dir, LEFT)) newDir = DIR8_LEFT;
		if (streq(dir, RIGHT)) newDir = DIR8_RIGHT;
		if (streq(dir, TOP)) newDir = DIR8_UP;
		if (streq(dir, BOTTOM)) newDir = DIR8_DOWN;
		connectNodes(name1, name2, newDir);
	}
}
