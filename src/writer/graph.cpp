#define NODES_MAX 128
#define LINES_MAX 128
#define NODE_NAME_MAX SHORT_STR

#define LINES_LAYER 40
#define NODES_LAYER 50

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
		MintSprite *bg;

		Node nodes[NODES_MAX];
		int nodesNum;

		MintSprite *lines[LINES_MAX];
		int linesNum;
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
		graph->linesNum = 0;

		{ /// Bg
			MintSprite *spr = createMintSprite();
			spr->setupRect(engine->width, engine->height, 0x000000);
			graph->bg = spr;
		}

		for (int i = 0; i < graph->nodesNum; i++) {
			Node *node = &graph->nodes[i];

			{ /// Node sprite
				MintSprite *spr = createMintSprite();
				spr->setupRect(128, 128, 0x555555);
				graph->bg->addChild(spr);
				spr->layer = lowestLayer + NODES_LAYER;

				node->sprite = spr;
			}

			{ /// Node text
				MintSprite *spr = createMintSprite();
				spr->setupEmpty(node->sprite->width, node->sprite->height);
				node->sprite->addChild(spr);
				spr->setText(node->name);
				spr->layer = lowestLayer + NODES_LAYER;
				spr->alignOutside(DIR8_CENTER);
			}

		}

		for (int i = 0; i < graph->nodesNum; i++) {
			Node *node = &graph->nodes[i];
			Node *other = node->connectedTo;
			if (other) {
				node->sprite->alignOutside(other->sprite, node->connectedDir, 50, 50);

				{ /// Line
					Point p1 = node->sprite->getCenterPoint();
					Point p2 = other->sprite->getCenterPoint();
					float dist = pointDistance(&p1, &p2);

					MintSprite *spr = createMintSprite();
					spr->setupRect(dist, 4, 0xFFFFFF);
					spr->centerPivot = true;
					spr->layer = lowestLayer + LINES_LAYER;
					spr->x = (p1.x + p2.x) / 2.0 - spr->width/2;
					spr->y = (p1.y + p2.y) / 2.0 - spr->height/2;
					spr->rotation = atan2(p2.y - p1.y, p2.x - p1.x) * 180 / M_PI;
					graph->bg->addChild(spr);

					graph->lines[graph->linesNum++] = spr;
				}
			}
		}
	}

	void hideGraph() {
		graph->bg->destroy();

		for (int i = 0; i < graph->nodesNum; i++) {
			Node *node = &graph->nodes[i];
			if (node->sprite) {
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
