#define NODES_MAX 128
#define NODE_NAME_MAX SHORT_STR

namespace Writer {

	void initGraph();
	void submitNode(const char *name, const char *passage);
	void attachNode(const char *name1, const char *name2, Dir8 dir);

	void showGraph();
	void updateGraph();
	void hideGraph();

	void js_submitNode(CScriptVar *v, void *userdata);
	void js_attachNode(CScriptVar *v, void *userdata);

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
		jsInterp->addNative("function attachNode(name1, name2, dir)", &js_attachNode, 0);
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
				node->sprite->alignOutside(other->sprite, node->connectedDir, 10, 10);
			}
		}
	}

	void hideGraph() {
		for (int i = 0; i < graph->nodesNum; i++) {
			Node *node = &graph->nodes[i];
			if (node->sprite) {
				node->sprite->destroy();
				node->sprite = NULL;
			}
		}
	}

	void updateGraph() {
		for (int i = 0; i < graph->nodesNum; i++) {
			Node *node = &graph->nodes[i];

			if (node->sprite && node->sprite->justPressed) {
				changeState(STATE_MOD);
				gotoPassage(node->passage);
			}
		}
	}

	void attachNode(const char *name1, const char *name2, Dir8 dir) {
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

	void js_attachNode(CScriptVar *v, void *userdata) {
		const char *name1 = v->getParameter("name1")->getString().c_str();
		const char *name2 = v->getParameter("name2")->getString().c_str();
		const char *dir = v->getParameter("dir")->getString().c_str();

		Dir8 newDir;
		if (streq(dir, LEFT)) newDir = DIR8_LEFT;
		if (streq(dir, RIGHT)) newDir = DIR8_RIGHT;
		if (streq(dir, TOP)) newDir = DIR8_UP;
		if (streq(dir, BOTTOM)) newDir = DIR8_DOWN;
		attachNode(name1, name2, newDir);
	}
}
