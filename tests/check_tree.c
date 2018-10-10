#include <check.h>
#include <string.h>
#include <stdlib.h>

#include "../src/tree.h"

#define EXAMPLE_TYPE_1 "foo_t"
#define EXAMPLE_TYPE_2 "bar_t"
#define EXAMPLE_TYPE_3 "baz_t"

struct av_rule * make_example_av_rule() {

	// allow foo_t { bar_t baz_t }:file { read write getattr };
	struct av_rule *av_rule_data = malloc(sizeof(struct av_rule));

	av_rule_data->flavor = AV_RULE_ALLOW;

	av_rule_data->sources = malloc(sizeof(struct string_list));
	ck_assert_ptr_nonnull(av_rule_data->sources);

	av_rule_data->sources->string = strdup(EXAMPLE_TYPE_1);
	ck_assert_ptr_nonnull(av_rule_data->sources->string);

	av_rule_data->sources->next = NULL;

	av_rule_data->targets = malloc(sizeof(struct string_list));
	ck_assert_ptr_nonnull(av_rule_data->targets);

	av_rule_data->targets->string = strdup(EXAMPLE_TYPE_2);
	ck_assert_ptr_nonnull(av_rule_data->targets->string);

	av_rule_data->targets->next = malloc(sizeof(struct string_list));
	ck_assert_ptr_nonnull(av_rule_data->targets->next);

	av_rule_data->targets->next->string = strdup(EXAMPLE_TYPE_3);
	ck_assert_ptr_nonnull(av_rule_data->targets->next->string);

	av_rule_data->targets->next->next = NULL;

	av_rule_data->object_classes = malloc(sizeof(struct string_list));
	ck_assert_ptr_nonnull(av_rule_data->object_classes);

	av_rule_data->object_classes->string = strdup("file");
	ck_assert_ptr_nonnull(av_rule_data->object_classes->string);

	av_rule_data->object_classes->next = NULL;

	av_rule_data->perms = malloc(sizeof(struct string_list));
	ck_assert_ptr_nonnull(av_rule_data->perms);

	av_rule_data->perms->string = strdup("read");
	ck_assert_ptr_nonnull(av_rule_data->perms->string);

	av_rule_data->perms->next = malloc(sizeof(struct string_list));
	ck_assert_ptr_nonnull(av_rule_data->perms->next);

	av_rule_data->perms->next->string = strdup("write");
	ck_assert_ptr_nonnull(av_rule_data->perms->next->string);

	av_rule_data->perms->next->next = malloc(sizeof(struct string_list));
	ck_assert_ptr_nonnull(av_rule_data->perms->next->next);

	av_rule_data->perms->next->next->string = strdup("getattr");
	ck_assert_ptr_nonnull(av_rule_data->perms->next->next->string);

	av_rule_data->perms->next->next->next = NULL;

	return av_rule_data;

}

START_TEST (test_insert_policy_node_av_rule) {

	struct policy_node parent_node;
	parent_node.parent = NULL;
	parent_node.next = NULL;
	parent_node.prev = NULL;
	parent_node.first_child = NULL;
	parent_node.flavor = NODE_TE_FILE;
	parent_node.data.av = NULL;

	union node_data av_data;
	av_data.av = make_example_av_rule();

	ck_assert_int_eq(SELINT_SUCCESS, insert_policy_node(&parent_node, NODE_AV_RULE, av_data));

	ck_assert_ptr_nonnull(parent_node.first_child);
	ck_assert_ptr_eq(parent_node.first_child->data.av, av_data.av);
	ck_assert_int_eq(parent_node.first_child->flavor, NODE_AV_RULE);

	ck_assert_int_eq(SELINT_SUCCESS, free_policy_node(parent_node.first_child));

}
END_TEST

int main(void) {
	return 0;
}
