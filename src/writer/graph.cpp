#define NODES_MAX 128
#define NODE_NAME_MAX SHORT_STR

namespace Writer {

	void initGraph();
	void submitNode(const char *name, const char *passage);
	void attachNode(const char *prev, const char *next, const char *dirStr);

	void showGraph();
	void updateGraph();
	void hideGraph();

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

	void attachNode(const char *prev, const char *next, const char *dirStr) {
		Node *node1 = NULL;
		Node *node2 = NULL;

		Dir8 dir = DIR8_CENTER;
		if (streq(dirStr, LEFT)) dir = DIR8_LEFT;
		else if (streq(dirStr, RIGHT)) dir = DIR8_RIGHT;
		else if (streq(dirStr, TOP)) dir = DIR8_UP;
		else if (streq(dirStr, BOTTOM)) dir = DIR8_DOWN;

		for (int i = 0; i < graph->nodesNum; i++)
			if (streq(graph->nodes[i].name, next))
				node1 = &graph->nodes[i];

		for (int i = 0; i < graph->nodesNum; i++)
			if (streq(graph->nodes[i].name, prev))
				node2 = &graph->nodes[i];

		Assert(node1);
		Assert(node2);

		node1->connectedTo = node2;
		node1->connectedDir = dir;
	}
}
