#include <stdlib.h>
#include <string.h>

#include "ordering.h"
#include "maps.h"

struct ordering_metadata *prepare_ordering_metadata(const struct policy_node *head)
{
	const struct policy_node *cur = head->first_child; // head is file.  Order the contents
	size_t count = 0;
	struct section_data *sections = calloc(1, sizeof(struct section_data));

	while (cur) {
		if (add_section_info(sections, get_section(cur), cur->lineno) == SELINT_BAD_ARG) {
			free(sections);
			return NULL;
		}
		count += 1;
		cur = dfs_next(cur);
	}
	calculate_average_lines(sections);

	struct ordering_metadata *ret = calloc(1, sizeof(struct ordering_metadata) +
	                                       (count * sizeof(struct order_node)));
	ret->order_node_len = count;
	ret->sections = sections;
	// The nodes array will be populated during the LIS traversal
	return ret;
}

void calculate_longest_increasing_subsequence(const struct policy_node *head,
                                              struct ordering_metadata *ordering,
                                              int (*comp_func)(const struct policy_node *first,
                                                               const struct policy_node *second))
{
	struct order_node *nodes = ordering->nodes;
	int longest_seq = 0;
	int index = 0;

	struct policy_node *cur = head->first_child;

	while (cur) {
		// Save the node in the array
		nodes[index].node = cur;

		// binary search sequences so far
		int low = 1;
		int high = longest_seq;
		while (low <= high) {
			int mid = (low + high + 1) / 2; // Ceiling
			if (comp_func(nodes[nodes[mid].end_of_seq].node, nodes[index].node) >= 0) {
				low = mid + 1;
			} else {
				high = mid - 1;
			}
		}

		// Now low should be 1 greater than the length of the longest
		// sequence that ends lower than the current number
		if (low <= 1) {
			nodes[index].seq_prev = -1; // No previous node
		} else {
			nodes[index].seq_prev = nodes[low - 2].end_of_seq;
		}
		nodes[low - 1].end_of_seq = index;

		if (low > longest_seq) {
			longest_seq = low;
		}
		index++;
		cur = dfs_next(cur);
	}

	// Mark LIS elements
	index = nodes[longest_seq - 1].end_of_seq;
	while (index != -1) {
		nodes[index].in_order = 1;
		index = nodes[index].seq_prev;
	}
}

enum selint_error add_section_info(struct section_data *sections,
                                   char *section_name,
                                   unsigned int lineno)
{
	if (sections == NULL || section_name == NULL) {
		return SELINT_BAD_ARG;
	}
	struct section_data *cur = sections;
	if (sections->section_name != NULL) {
		while (0 != strcmp(cur->section_name, section_name)) {
			if (cur->next == NULL) {
				cur->next = calloc(1, sizeof(struct section_data));
				cur = cur->next;
				break;
			}
			cur = cur->next;
		}
	}
	// cur is now the appropriate section_data node.  If section_name is
	// NULL, then this is a new node
	if (!cur->section_name) {
		cur->section_name = strdup(section_name);
	}

	cur->lineno_count++;
	cur->lines_sum += lineno;
	return SELINT_SUCCESS;
}

char *get_section(const struct policy_node *node)
{
	if (!node) {
		return NULL; //Error
	}

	switch (node->flavor) {
	case NODE_TE_FILE:
	case NODE_IF_FILE:
	case NODE_FC_FILE:
		return NULL; // Should never happen
	case NODE_AV_RULE:
		// The case of multiple source types is weird.  For now
		// just using the first one seems fine.
		return node->data.av_data->sources->string;
	case NODE_TT_RULE:
	case NODE_TM_RULE:
	case NODE_TC_RULE:
		// TODO: Are type_member and type_change the same as tt
		// from an ordering standpoint?
		// The case of multiple source types is weird.  For now
		// just using the first one seems fine.
		return node->data.av_data->sources->string;
	case NODE_ROLE_ALLOW:
		// This is not in the style guide. I normally see it grouped
		// with declarations, but maybe a future ordering configuration
		// can sort it that way
		return "_non_ordered";
	case NODE_DECL:
	case NODE_ALIAS:
	case NODE_TYPE_ALIAS:
	case NODE_TYPE_ATTRIBUTE:
		return "_declarations";
	case NODE_M4_CALL:
		return "_non_ordered"; // TODO: It's probably way more
	// complicated than this
	case NODE_OPTIONAL_POLICY:
	case NODE_OPTIONAL_ELSE:
		return get_section(node->first_child);
	case NODE_M4_ARG:
		return "_non_ordered"; //TODO
	case NODE_START_BLOCK:
		return get_section(node->next);
	case NODE_IF_CALL:
		return node->data.ic_data->args->string; // TODO: transform interfaces
	// go in _declarations
	case NODE_TEMP_DEF:
	case NODE_IF_DEF:
		return NULL; // if files only
	case NODE_REQUIRE:
	case NODE_GEN_REQ:
		return "_non_ordered"; // Not in style guide
	case NODE_PERMISSIVE:
		return "_non_ordered"; // Not in style guide
	case NODE_FC_ENTRY:
		return NULL;           // fc files only
	case NODE_COMMENT:
	case NODE_EMPTY:
	case NODE_ERROR:
		return "_non_ordered";
	default:
		// Should never happen
		return NULL;
	}
}

void calculate_average_lines(struct section_data *sections)
{
	while (sections) {
		sections->avg_line = (float)sections->lines_sum / (float)sections->lineno_count;
		sections = sections->next;
	}
}

float get_avg_line_by_name(char *section_name, struct section_data *sections)
{
	while (0 != strcmp(sections->section_name, section_name)) {
		sections = sections->next;
		if (!sections) {
			return -1; //Error
		}
	}
	return sections->avg_line;
}

int is_self_rule(const struct policy_node *node)
{
	return node->flavor == NODE_AV_RULE &&
	       node->data.av_data &&
	       node->data.av_data->targets &&
	       0 == strcmp(node->data.av_data->targets->string, "self");
}

int is_own_module_rule(const struct policy_node *node)
{
	if (node->flavor != NODE_AV_RULE &&
	    node->flavor != NODE_IF_CALL) {
		return 0;
	}
	char *domain_name = get_section(node);
	if (!domain_name) {
		return 0;
	}
	char *current_mod = look_up_in_decl_map(domain_name, DECL_TYPE);
	if (!current_mod) {
		current_mod = look_up_in_decl_map(domain_name, DECL_ATTRIBUTE);
	}
	if (node->flavor == NODE_IF_CALL) {
		// These should actually be patterns, not real calls
		if (look_up_in_ifs_map(node->data.ic_data->name)) {
			return 0;
		}
	}
	struct string_list *types = get_types_in_node(node);
	struct string_list *cur = types;
	while (cur) {
		char *module_of_type_or_attr = look_up_in_decl_map(cur->string, DECL_TYPE);
		if (!module_of_type_or_attr) {
			module_of_type_or_attr = look_up_in_decl_map(cur->string, DECL_ATTRIBUTE);
		}
		if (module_of_type_or_attr &&
		    0 != strcmp(module_of_type_or_attr, current_mod)) {
			free_string_list(types);
			return 0;
		}
		cur = cur->next;
	}
	free_string_list(types);
	// This assumes that not found strings are not types from other modules.
	// This is probably necessary because we'll find strings like "file" or
	// "read_file_perms" for example.  However, in normal mode without context
	// this could definitely be a problem because we won't find types from
	// other modules
	return 1;
}

int is_kernel_layer_if_call(const struct policy_node *node)
{
	return 0; //TODO
}

int is_system_layer_if_call(const struct policy_node *node)
{
	return 0; //TODO
}

enum local_subsection get_local_subsection(const struct policy_node *node)
{
	if (!node) {
		return LSS_UNKNOWN;
	}
	if (is_self_rule(node)) {
		return LSS_SELF;
	} else if (is_own_module_rule(node)) {
		return LSS_OWN;
	} else if (is_kernel_layer_if_call(node)) {
		return LSS_KERNEL;
	} else if (is_system_layer_if_call(node)) {
		return LSS_SYSTEM;
	} else if (node->flavor == NODE_IF_CALL) {
		return LSS_OTHER;
	} else {
		// TODO conditional, optional etc
		return LSS_UNKNOWN;
	}
}

#define CHECK_ORDERING(to_check_first, to_check_second, comp, ret) \
	if (to_check_first == comp) { \
		return ret; \
	} \
	if (to_check_second == comp) { \
		return -ret; \
	} \
// Call this in order of an ordering on enums.  It returns a positve or
// negative value based on which one it encounters first.
#define CHECK_FLAVOR_ORDERING(data_flavor, comp, ret) \
	CHECK_ORDERING(first->data.data_flavor->flavor, second->data.data_flavor->flavor, comp, ret)

enum order_difference_reason compare_nodes_refpolicy(struct ordering_metadata *ordering_data,
                                                     const struct policy_node *first,
                                                     const struct policy_node *second)
{
	char *first_section_name = get_section(first);
	char *second_section_name = get_section(second);

	if (first_section_name == NULL || second_section_name == NULL) {
		return ORDERING_ERROR;
	}
	if (0 != strcmp(first_section_name, second_section_name)) {
		if ((get_avg_line_by_name(first_section_name, ordering_data->sections) >
		     get_avg_line_by_name(second_section_name, ordering_data->sections)) ||
		    0 == strcmp(second_section_name, "_declarations")) {
			return -ORDER_SECTION;
		} else {
			return ORDER_SECTION;
		}
	}

	// If we made it to this point the two nodes are in the same section

	if (0 == strcmp(first_section_name, "_declarations")) {
		if (first->flavor == NODE_DECL && second->flavor == NODE_DECL) {
			if (first->data.d_data->flavor != second->data.d_data->flavor) {
				CHECK_FLAVOR_ORDERING(d_data, DECL_BOOL, ORDER_DECLARATION_SUBSECTION);
				CHECK_FLAVOR_ORDERING(d_data, DECL_ATTRIBUTE, ORDER_DECLARATION_SUBSECTION);
				CHECK_FLAVOR_ORDERING(d_data, DECL_TYPE, ORDER_DECLARATION_SUBSECTION);
			} else {
				// TODO: same subsection
			}
		}
	}

	// Local policy rules sections
	enum local_subsection lss_first = get_local_subsection(first);
	enum local_subsection lss_second = get_local_subsection(second);

	if (lss_first == LSS_UNKNOWN || lss_second == LSS_UNKNOWN) {
		return ORDER_EQUAL; // ... Maybe? Should this case be handled earlier?
	}

	CHECK_ORDERING(lss_first, lss_second, LSS_SELF, ORDER_LOCAL_SUBSECTION);
	CHECK_ORDERING(lss_first, lss_second, LSS_OWN, ORDER_LOCAL_SUBSECTION);
	CHECK_ORDERING(lss_first, lss_second, LSS_KERNEL, ORDER_LOCAL_SUBSECTION);
	CHECK_ORDERING(lss_first, lss_second, LSS_SYSTEM, ORDER_LOCAL_SUBSECTION);
	CHECK_ORDERING(lss_first, lss_second, LSS_OTHER, ORDER_LOCAL_SUBSECTION);
	CHECK_ORDERING(lss_first, lss_second, LSS_BUILD_OPTION, ORDER_LOCAL_SUBSECTION);
	CHECK_ORDERING(lss_first, lss_second, LSS_CONDITIONAL, ORDER_LOCAL_SUBSECTION);
	CHECK_ORDERING(lss_first, lss_second, LSS_TUNABLE, ORDER_LOCAL_SUBSECTION);
	CHECK_ORDERING(lss_first, lss_second, LSS_OPTIONAL, ORDER_LOCAL_SUBSECTION);

	// TODO: alphabetical

	return ORDER_EQUAL;
}

void free_ordering_metadata(struct ordering_metadata *to_free)
{
	free_section_data(to_free->sections);
	free(to_free);
}

void free_section_data(struct section_data *to_free)
{
	if (to_free == NULL) {
		return;
	}
	free(to_free->section_name);
	free_section_data(to_free->next);
	free(to_free);
}